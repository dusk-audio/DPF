// Copyright 2012-2023 David Robillard <d@drobilla.net>
// Copyright 2025 DISTRHO Plugin Framework contributors
// SPDX-License-Identifier: ISC

/*
  Cairo backend for the Wayland platform, over wl_shm.

  There is no "Cairo surface for a Wayland window" the way there is for X11, so this draws into a
  plain image surface backed by shared memory and hands the compositor the resulting wl_buffer.

  Two buffers are used and alternated, because a buffer that has been attached stays owned by the
  compositor until it sends wl_buffer.release.  Since the two buffers hold different frames, a
  partial expose paints the previous frame over the new buffer first, so that the parts the
  application does not redraw are still correct.
*/

#include "../pugl-upstream/src/macros.h"
#include "../pugl-upstream/src/types.h"
#include "wayland.h"

#include "pugl/cairo.h"
#include "pugl/pugl.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define PUGL_WAYLAND_CAIRO_NUM_BUFFERS 2

typedef struct {
  int                 fd;
  size_t              poolSize;
  uint8_t*            data;
  struct wl_shm_pool* pool;
  struct wl_buffer*   buffers[PUGL_WAYLAND_CAIRO_NUM_BUFFERS];
  cairo_surface_t*    images[PUGL_WAYLAND_CAIRO_NUM_BUFFERS];
  bool                busy[PUGL_WAYLAND_CAIRO_NUM_BUFFERS];
  cairo_t*            cr;
  PuglArea            size;
  int                 stride;
  int                 index;
  int                 lastIndex;
} PuglWaylandCairoSurface;

static void
puglWaylandCairoBufferRelease(void* const             data,
                              struct wl_buffer* const PUGL_UNUSED(buffer))
{
  *(bool*)data = false;
}

static const struct wl_buffer_listener puglWaylandCairoBufferListener = {
  puglWaylandCairoBufferRelease};

/// Create an anonymous file suitable for a wl_shm pool
static int
puglWaylandCairoCreateShmFile(const size_t size)
{
  int fd = -1;

#if defined(__linux__) && defined(MFD_CLOEXEC)
  fd = memfd_create("pugl-shm", MFD_CLOEXEC);
#endif

  if (fd < 0) {
    const char* const dir = getenv("XDG_RUNTIME_DIR");
    if (!dir) {
      return -1;
    }

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/pugl-shm-XXXXXX", dir);

    fd = mkstemp(path);
    if (fd < 0) {
      return -1;
    }

    unlink(path);
    fcntl(fd, F_SETFD, FD_CLOEXEC);
  }

  if (ftruncate(fd, (off_t)size) != 0) {
    close(fd);
    return -1;
  }

  return fd;
}

static void
puglWaylandCairoClosePool(PuglWaylandCairoSurface* const surface)
{
  for (int i = 0; i < PUGL_WAYLAND_CAIRO_NUM_BUFFERS; ++i) {
    if (surface->images[i]) {
      cairo_surface_destroy(surface->images[i]);
      surface->images[i] = NULL;
    }
    if (surface->buffers[i]) {
      wl_buffer_destroy(surface->buffers[i]);
      surface->buffers[i] = NULL;
    }
    surface->busy[i] = false;
  }

  if (surface->pool) {
    wl_shm_pool_destroy(surface->pool);
    surface->pool = NULL;
  }

  if (surface->data) {
    munmap(surface->data, surface->poolSize);
    surface->data = NULL;
  }

  if (surface->fd >= 0) {
    close(surface->fd);
    surface->fd = -1;
  }

  surface->poolSize  = 0;
  surface->lastIndex = -1;
}

static PuglStatus
puglWaylandCairoOpenPool(PuglView* const view, const PuglArea size)
{
  PuglWaylandCairoSurface* const surface =
    (PuglWaylandCairoSurface*)view->impl->surface;

  struct wl_shm* const shm = view->world->impl->shm;

  puglWaylandCairoClosePool(surface);

  const int stride =
    cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, size.width);

  if (stride <= 0) {
    return PUGL_BAD_CONFIGURATION;
  }

  const size_t bufferSize = (size_t)stride * size.height;
  const size_t poolSize   = bufferSize * PUGL_WAYLAND_CAIRO_NUM_BUFFERS;

  const int fd = puglWaylandCairoCreateShmFile(poolSize);
  if (fd < 0) {
    return PUGL_CREATE_CONTEXT_FAILED;
  }

  uint8_t* const data = (uint8_t*)mmap(
    NULL, poolSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  if (data == MAP_FAILED) {
    close(fd);
    return PUGL_CREATE_CONTEXT_FAILED;
  }

  surface->fd       = fd;
  surface->data     = data;
  surface->poolSize = poolSize;
  surface->stride   = stride;
  surface->size     = size;
  surface->pool     = wl_shm_create_pool(shm, fd, (int32_t)poolSize);

  if (!surface->pool) {
    puglWaylandCairoClosePool(surface);
    return PUGL_CREATE_CONTEXT_FAILED;
  }

  for (int i = 0; i < PUGL_WAYLAND_CAIRO_NUM_BUFFERS; ++i) {
    surface->buffers[i] = wl_shm_pool_create_buffer(surface->pool,
                                                    (int32_t)(bufferSize * i),
                                                    size.width,
                                                    size.height,
                                                    stride,
                                                    WL_SHM_FORMAT_ARGB8888);

    if (!surface->buffers[i]) {
      puglWaylandCairoClosePool(surface);
      return PUGL_CREATE_CONTEXT_FAILED;
    }

    wl_buffer_add_listener(surface->buffers[i],
                           &puglWaylandCairoBufferListener,
                           &surface->busy[i]);

    surface->images[i] = cairo_image_surface_create_for_data(
      data + (bufferSize * i),
      CAIRO_FORMAT_ARGB32,
      size.width,
      size.height,
      stride);

    if (cairo_surface_status(surface->images[i])) {
      puglWaylandCairoClosePool(surface);
      return PUGL_CREATE_CONTEXT_FAILED;
    }
  }

  return PUGL_SUCCESS;
}

static PuglStatus
puglWaylandCairoConfigure(PuglView* const view)
{
  return view->world->impl->shm ? PUGL_SUCCESS : PUGL_BACKEND_FAILED;
}

static PuglStatus
puglWaylandCairoCreate(PuglView* const view)
{
  PuglWaylandCairoSurface* const surface =
    (PuglWaylandCairoSurface*)calloc(1, sizeof(PuglWaylandCairoSurface));

  if (!surface) {
    return PUGL_NO_MEMORY;
  }

  surface->fd        = -1;
  surface->lastIndex = -1;
  view->impl->surface = surface;

  return PUGL_SUCCESS;
}

static void
puglWaylandCairoDestroy(PuglView* const view)
{
  PuglWaylandCairoSurface* const surface =
    (PuglWaylandCairoSurface*)view->impl->surface;

  if (!surface) {
    return;
  }

  if (surface->cr) {
    cairo_destroy(surface->cr);
    surface->cr = NULL;
  }

  puglWaylandCairoClosePool(surface);
  free(surface);
  view->impl->surface = NULL;
}

static PuglStatus
puglWaylandCairoEnter(PuglView* const view, const PuglExposeEvent* const expose)
{
  PuglWaylandCairoSurface* const surface =
    (PuglWaylandCairoSurface*)view->impl->surface;

  if (!surface || !expose) {
    return PUGL_SUCCESS;
  }

  const PuglArea size = puglWaylandGetBufferSize(view);

  if (!puglIsValidArea(size)) {
    return PUGL_FAILURE;
  }

  if (!surface->pool || size.width != surface->size.width ||
      size.height != surface->size.height) {
    const PuglStatus st = puglWaylandCairoOpenPool(view, size);
    if (st) {
      return st;
    }
  }

  // Pick a buffer the compositor is not holding, falling back to reuse rather than stalling
  int index = -1;
  for (int i = 0; i < PUGL_WAYLAND_CAIRO_NUM_BUFFERS; ++i) {
    if (!surface->busy[i]) {
      index = i;
      break;
    }
  }

  if (index < 0) {
    index = 0;
  }

  surface->index = index;
  surface->cr    = cairo_create(surface->images[index]);

  if (cairo_status(surface->cr)) {
    cairo_destroy(surface->cr);
    surface->cr = NULL;
    return PUGL_CREATE_CONTEXT_FAILED;
  }

  /* Carry over whatever the last frame drew: this buffer holds an older frame, and the application
     is only required to redraw the exposed region. */
  if (surface->lastIndex >= 0 && surface->lastIndex != index) {
    cairo_save(surface->cr);
    cairo_set_operator(surface->cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface(surface->cr, surface->images[surface->lastIndex], 0, 0);
    cairo_paint(surface->cr);
    cairo_restore(surface->cr);
  }

  cairo_rectangle(
    surface->cr, expose->x, expose->y, expose->width, expose->height);
  cairo_clip(surface->cr);

  return PUGL_SUCCESS;
}

static PuglStatus
puglWaylandCairoLeave(PuglView* const view, const PuglExposeEvent* const expose)
{
  PuglWaylandCairoSurface* const surface =
    (PuglWaylandCairoSurface*)view->impl->surface;
  struct wl_surface* const wlSurface = view->impl->wlSurface;

  if (!surface || !expose) {
    return PUGL_SUCCESS;
  }

  if (surface->cr) {
    cairo_destroy(surface->cr);
    surface->cr = NULL;
  }

  const int index = surface->index;

  if (!surface->images[index] || !wlSurface) {
    return PUGL_FAILURE;
  }

  cairo_surface_flush(surface->images[index]);

  wl_surface_attach(wlSurface, surface->buffers[index], 0, 0);

#if defined(WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION)
  if (wl_surface_get_version(wlSurface) >=
      WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION) {
    wl_surface_damage_buffer(
      wlSurface, expose->x, expose->y, expose->width, expose->height);
  } else
#endif
  {
    wl_surface_damage(
      wlSurface, expose->x, expose->y, expose->width, expose->height);
  }

  wl_surface_commit(wlSurface);

  surface->busy[index] = true;
  surface->lastIndex   = index;

  return PUGL_SUCCESS;
}

static void*
puglWaylandCairoGetContext(PuglView* const view)
{
  PuglWaylandCairoSurface* const surface =
    (PuglWaylandCairoSurface*)view->impl->surface;

  return surface ? surface->cr : NULL;
}

const PuglBackend*
puglCairoBackend(void)
{
  static const PuglBackend backend = {puglWaylandCairoConfigure,
                                      puglWaylandCairoCreate,
                                      puglWaylandCairoDestroy,
                                      puglWaylandCairoEnter,
                                      puglWaylandCairoLeave,
                                      puglWaylandCairoGetContext};

  return &backend;
}
