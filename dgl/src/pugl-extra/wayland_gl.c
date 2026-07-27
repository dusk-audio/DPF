// Copyright 2012-2022 David Robillard <d@drobilla.net>
// Copyright 2025 DISTRHO Plugin Framework contributors
// SPDX-License-Identifier: ISC

/*
  OpenGL backend for the Wayland platform, over EGL and wl_egl_window.

  Two notes worth keeping in mind while reading this:

  * eglSwapBuffers() is what commits the surface.  wayland.c requests its frame callback just before
    the backend leaves the context, so the request rides along with that same commit.
  * The drawable is resized lazily in enter() rather than from a resize hook, because PuglBackend has
    no such hook and the x11/win backends do not need one (their drawables follow the window).
*/

#include "../pugl-upstream/src/stub.h"
#include "../pugl-upstream/src/types.h"
#include "wayland.h"

#include "pugl/gl.h"
#include "pugl/pugl.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
  EGLDisplay            display;
  EGLConfig             config;
  EGLContext            context;
  EGLSurface            surface;
  struct wl_egl_window* window;
  PuglArea              size;
} PuglWaylandGlSurface;

/* EGL does not accept EGL_DONT_CARE for the "at least this many bits" attributes, and does not need
   to: zero already means "no preference" for all of them. */
static EGLint
puglWaylandGlHintValue(const int value)
{
  return value == PUGL_DONT_CARE ? 0 : (EGLint)value;
}

static int
puglWaylandGlGetAttrib(const EGLDisplay display,
                       const EGLConfig  config,
                       const EGLint     attrib)
{
  EGLint value = 0;
  eglGetConfigAttrib(display, config, attrib, &value);
  return (int)value;
}

/// Open the EGL display for a wl_display, preferring the platform entry points
static EGLDisplay
puglWaylandGlOpenDisplay(struct wl_display* const wlDisplay)
{
#if defined(EGL_VERSION_1_5)
  {
    EGLDisplay display =
      eglGetPlatformDisplay(EGL_PLATFORM_WAYLAND_KHR, wlDisplay, NULL);
    if (display != EGL_NO_DISPLAY) {
      return display;
    }
  }
#endif

  {
    // EGL 1.4 with EGL_EXT_platform_wayland
    typedef EGLDisplay(EGLAPIENTRYP PFNEGLGETPLATFORMDISPLAYEXT)(
      EGLenum platform, void* nativeDisplay, const EGLint* attribList);

    PFNEGLGETPLATFORMDISPLAYEXT getPlatformDisplay =
      (PFNEGLGETPLATFORMDISPLAYEXT)eglGetProcAddress(
        "eglGetPlatformDisplayEXT");

    if (getPlatformDisplay) {
      EGLDisplay display =
        getPlatformDisplay(EGL_PLATFORM_WAYLAND_KHR, wlDisplay, NULL);
      if (display != EGL_NO_DISPLAY) {
        return display;
      }
    }
  }

  return eglGetDisplay((EGLNativeDisplayType)wlDisplay);
}

/**
   Acquire the world's shared EGLDisplay, initialising it on first use.

   There is exactly one EGLDisplay per wl_display no matter how many windows exist, so it is
   reference counted in the world rather than owned by a view: eglTerminate() destroys every context
   and surface belonging to the display, which would break sibling views.
*/
static EGLDisplay
puglWaylandGlAcquireDisplay(PuglWorldInternals* const wimpl)
{
  if (wimpl->eglDisplay) {
    ++wimpl->eglRefCount;
    return (EGLDisplay)wimpl->eglDisplay;
  }

  const EGLDisplay display = puglWaylandGlOpenDisplay(wimpl->display);
  if (display == EGL_NO_DISPLAY) {
    return EGL_NO_DISPLAY;
  }

  EGLint major = 0;
  EGLint minor = 0;
  if (eglInitialize(display, &major, &minor) != EGL_TRUE) {
    return EGL_NO_DISPLAY;
  }

  wimpl->eglDisplay  = display;
  wimpl->eglRefCount = 1U;
  return display;
}

static void
puglWaylandGlReleaseDisplay(PuglWorldInternals* const wimpl)
{
  if (!wimpl->eglDisplay || !wimpl->eglRefCount) {
    return;
  }

  if (--wimpl->eglRefCount == 0U) {
    eglTerminate((EGLDisplay)wimpl->eglDisplay);
    wimpl->eglDisplay = NULL;
  }
}

static PuglStatus
puglWaylandGlConfigure(PuglView* const view)
{
  PuglInternals* const      impl  = view->impl;
  PuglWorldInternals* const wimpl = view->world->impl;
  const bool                isGles =
    view->hints[PUGL_CONTEXT_API] == PUGL_OPENGL_ES_API;

  const EGLDisplay display = puglWaylandGlAcquireDisplay(wimpl);
  if (display == EGL_NO_DISPLAY) {
    return PUGL_CREATE_CONTEXT_FAILED;
  }

  if (eglBindAPI(isGles ? EGL_OPENGL_ES_API : EGL_OPENGL_API) != EGL_TRUE) {
    puglWaylandGlReleaseDisplay(wimpl);
    return PUGL_CREATE_CONTEXT_FAILED;
  }

  EGLint renderableType = EGL_OPENGL_BIT;
  if (isGles) {
    renderableType = (view->hints[PUGL_CONTEXT_VERSION_MAJOR] >= 3)
                       ? EGL_OPENGL_ES3_BIT
                       : EGL_OPENGL_ES2_BIT;
  }

  // clang-format off
  const EGLint attrs[] = {
    EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
    EGL_RENDERABLE_TYPE, renderableType,
    EGL_RED_SIZE,        puglWaylandGlHintValue(view->hints[PUGL_RED_BITS]),
    EGL_GREEN_SIZE,      puglWaylandGlHintValue(view->hints[PUGL_GREEN_BITS]),
    EGL_BLUE_SIZE,       puglWaylandGlHintValue(view->hints[PUGL_BLUE_BITS]),
    EGL_ALPHA_SIZE,      puglWaylandGlHintValue(view->hints[PUGL_ALPHA_BITS]),
    EGL_DEPTH_SIZE,      puglWaylandGlHintValue(view->hints[PUGL_DEPTH_BITS]),
    EGL_STENCIL_SIZE,    puglWaylandGlHintValue(view->hints[PUGL_STENCIL_BITS]),
    EGL_SAMPLES,         puglWaylandGlHintValue(view->hints[PUGL_SAMPLES]),
    EGL_NONE
  };
  // clang-format on

  EGLConfig config     = NULL;
  EGLint    numConfigs = 0;

  if (eglChooseConfig(display, attrs, &config, 1, &numConfigs) != EGL_TRUE ||
      numConfigs < 1) {
    puglWaylandGlReleaseDisplay(wimpl);
    return PUGL_SET_FORMAT_FAILED;
  }

  PuglWaylandGlSurface* const surface =
    (PuglWaylandGlSurface*)calloc(1, sizeof(PuglWaylandGlSurface));

  if (!surface) {
    puglWaylandGlReleaseDisplay(wimpl);
    return PUGL_NO_MEMORY;
  }

  surface->display = display;
  surface->config  = config;
  surface->context = EGL_NO_CONTEXT;
  surface->surface = EGL_NO_SURFACE;
  impl->surface    = surface;

  view->hints[PUGL_RED_BITS] =
    puglWaylandGlGetAttrib(display, config, EGL_RED_SIZE);
  view->hints[PUGL_GREEN_BITS] =
    puglWaylandGlGetAttrib(display, config, EGL_GREEN_SIZE);
  view->hints[PUGL_BLUE_BITS] =
    puglWaylandGlGetAttrib(display, config, EGL_BLUE_SIZE);
  view->hints[PUGL_ALPHA_BITS] =
    puglWaylandGlGetAttrib(display, config, EGL_ALPHA_SIZE);
  view->hints[PUGL_DEPTH_BITS] =
    puglWaylandGlGetAttrib(display, config, EGL_DEPTH_SIZE);
  view->hints[PUGL_STENCIL_BITS] =
    puglWaylandGlGetAttrib(display, config, EGL_STENCIL_SIZE);
  view->hints[PUGL_SAMPLES] =
    puglWaylandGlGetAttrib(display, config, EGL_SAMPLES);

  // EGL window surfaces are always double buffered
  view->hints[PUGL_DOUBLE_BUFFER] = PUGL_TRUE;

  return PUGL_SUCCESS;
}

static PuglStatus
puglWaylandGlCreate(PuglView* const view)
{
  PuglInternals* const        impl    = view->impl;
  PuglWaylandGlSurface* const surface = (PuglWaylandGlSurface*)impl->surface;

  if (!surface || !impl->wlSurface) {
    return PUGL_BACKEND_FAILED;
  }

  const bool isGles = view->hints[PUGL_CONTEXT_API] == PUGL_OPENGL_ES_API;
  const PuglArea size = puglWaylandGetBufferSize(view);

  // clang-format off
  const EGLint contextAttrs[] = {
    EGL_CONTEXT_MAJOR_VERSION, view->hints[PUGL_CONTEXT_VERSION_MAJOR],
    EGL_CONTEXT_MINOR_VERSION, view->hints[PUGL_CONTEXT_VERSION_MINOR],
    EGL_NONE
  };

  // EGL 1.4 and OpenGL ES only understand the older single-number attribute
  const EGLint legacyContextAttrs[] = {
    EGL_CONTEXT_CLIENT_VERSION, view->hints[PUGL_CONTEXT_VERSION_MAJOR],
    EGL_NONE
  };
  // clang-format on

  surface->context = eglCreateContext(surface->display,
                                      surface->config,
                                      EGL_NO_CONTEXT,
                                      isGles ? legacyContextAttrs
                                             : contextAttrs);

  if (surface->context == EGL_NO_CONTEXT) {
    surface->context = eglCreateContext(
      surface->display, surface->config, EGL_NO_CONTEXT, legacyContextAttrs);
  }

  if (surface->context == EGL_NO_CONTEXT) {
    return PUGL_CREATE_CONTEXT_FAILED;
  }

  surface->window =
    wl_egl_window_create(impl->wlSurface, size.width, size.height);

  if (!surface->window) {
    return PUGL_CREATE_CONTEXT_FAILED;
  }

  surface->size    = size;
  surface->surface = eglCreateWindowSurface(
    surface->display, surface->config, (EGLNativeWindowType)surface->window, NULL);

  if (surface->surface == EGL_NO_SURFACE) {
    return PUGL_CREATE_CONTEXT_FAILED;
  }

  /* Pacing is done by wayland.c with wl_surface frame callbacks, so EGL is told not to block in
     eglSwapBuffers unless the application explicitly asked for a swap interval.  Letting both
     throttles run would add a frame of latency for no benefit. */
  if (eglMakeCurrent(surface->display,
                     surface->surface,
                     surface->surface,
                     surface->context) == EGL_TRUE) {
    const int hint = view->hints[PUGL_SWAP_INTERVAL];
    eglSwapInterval(surface->display, hint == PUGL_DONT_CARE ? 0 : hint);
    eglMakeCurrent(
      surface->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  }

  return PUGL_SUCCESS;
}

static void
puglWaylandGlDestroy(PuglView* const view)
{
  PuglWaylandGlSurface* const surface =
    (PuglWaylandGlSurface*)view->impl->surface;

  if (!surface) {
    return;
  }

  eglMakeCurrent(
    surface->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

  if (surface->surface != EGL_NO_SURFACE) {
    eglDestroySurface(surface->display, surface->surface);
  }
  if (surface->context != EGL_NO_CONTEXT) {
    eglDestroyContext(surface->display, surface->context);
  }
  if (surface->window) {
    wl_egl_window_destroy(surface->window);
  }

  free(surface);
  view->impl->surface = NULL;

  puglWaylandGlReleaseDisplay(view->world->impl);
}

PUGL_WARN_UNUSED_RESULT static PuglStatus
puglWaylandGlEnter(PuglView* const view, const PuglExposeEvent* const expose)
{
  PuglWaylandGlSurface* const surface =
    (PuglWaylandGlSurface*)view->impl->surface;

  if (!surface || surface->context == EGL_NO_CONTEXT ||
      surface->surface == EGL_NO_SURFACE) {
    return PUGL_FAILURE;
  }

  // Catch up with any resize that happened since the last time we drew
  const PuglArea size = puglWaylandGetBufferSize(view);
  if (size.width != surface->size.width ||
      size.height != surface->size.height) {
    wl_egl_window_resize(surface->window, size.width, size.height, 0, 0);
    surface->size = size;
  }

  if (eglMakeCurrent(surface->display,
                     surface->surface,
                     surface->surface,
                     surface->context) != EGL_TRUE) {
    return PUGL_FAILURE;
  }

  (void)expose;
  return PUGL_SUCCESS;
}

PUGL_WARN_UNUSED_RESULT static PuglStatus
puglWaylandGlLeave(PuglView* const view, const PuglExposeEvent* const expose)
{
  PuglWaylandGlSurface* const surface =
    (PuglWaylandGlSurface*)view->impl->surface;

  if (!surface) {
    return PUGL_FAILURE;
  }

  if (expose) {
    // This is what commits the wl_surface, frame callback request included
    eglSwapBuffers(surface->display, surface->surface);
  }

  return eglMakeCurrent(surface->display,
                        EGL_NO_SURFACE,
                        EGL_NO_SURFACE,
                        EGL_NO_CONTEXT) == EGL_TRUE
           ? PUGL_SUCCESS
           : PUGL_FAILURE;
}

PuglGlFunc
puglGetProcAddress(const char* const name)
{
  return (PuglGlFunc)eglGetProcAddress(name);
}

PuglStatus
puglEnterContext(PuglView* const view)
{
  return view->backend->enter(view, NULL);
}

PuglStatus
puglLeaveContext(PuglView* const view)
{
  return view->backend->leave(view, NULL);
}

const PuglBackend*
puglGlBackend(void)
{
  static const PuglBackend backend = {puglWaylandGlConfigure,
                                      puglWaylandGlCreate,
                                      puglWaylandGlDestroy,
                                      puglWaylandGlEnter,
                                      puglWaylandGlLeave,
                                      puglStubGetContext};

  return &backend;
}
