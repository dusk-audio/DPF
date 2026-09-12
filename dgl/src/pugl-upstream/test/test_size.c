// Copyright 2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

// Tests basic view setup

#undef NDEBUG

#include <puglutil/test_utils.h>

#include <pugl/pugl.h>
#include <pugl/stub.h>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef _WIN32
#  include <windows.h>

static WNDPROC originalWindowProc = NULL;
static unsigned nativeSizeEvents = 0;

static LRESULT CALLBACK
observeWindowSize(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
  if (message == WM_SIZE) {
    ++nativeSizeEvents;
  }
  return CallWindowProc(originalWindowProc, window, message, wParam, lParam);
}
#endif

typedef enum {
  START,
  REALIZED,
  CONFIGURED,
  UNREALIZED,
} State;

typedef struct {
  PuglWorld*      world;
  PuglView*       view;
  PuglTestOptions opts;
  State           state;
  PuglPoint       configuredPos;
  PuglArea        configuredSize;
} PuglTest;

static PuglStatus
onEvent(PuglView* view, const PuglEvent* event)
{
  PuglTest* test = (PuglTest*)puglGetHandle(view);

  if (test->opts.verbose) {
    printEvent(event, "Event: ", true);
  }

  switch (event->type) {
  case PUGL_REALIZE:
    assert(test->state == START);
    test->state = REALIZED;
    break;
  case PUGL_CONFIGURE:
    if (test->state == REALIZED) {
      test->state = CONFIGURED;
    }
    test->configuredPos.x       = event->configure.x;
    test->configuredPos.y       = event->configure.y;
    test->configuredSize.width  = event->configure.width;
    test->configuredSize.height = event->configure.height;
    break;
  case PUGL_UNREALIZE:
    test->state = UNREALIZED;
    break;
  default:
    break;
  }

  return PUGL_SUCCESS;
}

int
main(int argc, char** argv)
{
  static const PuglSpan minSize     = 128;
  static const PuglSpan defaultSize = 256;
  static const PuglSpan maxSize     = 512;

  PuglTest test = {puglNewWorld(PUGL_PROGRAM, 0),
                   NULL,
                   puglParseTestOptions(&argc, &argv),
                   START,
                   {0, 0},
                   {0U, 0U}};

  // Set up view with size bounds and an aspect ratio
  test.view = puglNewView(test.world);
  puglSetWorldString(test.world, PUGL_CLASS_NAME, "PuglTest");
  puglSetViewString(test.view, PUGL_WINDOW_TITLE, "Pugl Size Test");
  puglSetBackend(test.view, puglStubBackend());
  puglSetHandle(test.view, &test);
  puglSetEventFunc(test.view, onEvent);
  puglSetViewHint(test.view, PUGL_RESIZABLE, PUGL_TRUE);
  puglSetSizeHint(test.view, PUGL_DEFAULT_SIZE, defaultSize, defaultSize);
  puglSetSizeHint(test.view, PUGL_MIN_SIZE, minSize, minSize);
  puglSetSizeHint(test.view, PUGL_MAX_SIZE, maxSize, maxSize);
  puglSetSizeHint(test.view, PUGL_FIXED_ASPECT, 1, 1);
  puglSetPositionHint(test.view, PUGL_DEFAULT_POSITION, 384, 384);

  // Create and show window
  assert(!puglRealize(test.view));
  assert(puglShow(test.view, PUGL_SHOW_RAISE) <= PUGL_FAILURE);
  while (test.state < CONFIGURED) {
    assert(!puglUpdate(test.world, -1.0));
  }

  // Check that the frame matches the last configure event
  const PuglPoint pos  = puglGetPositionHint(test.view, PUGL_CURRENT_POSITION);
  const PuglArea  size = puglGetSizeHint(test.view, PUGL_CURRENT_SIZE);
  assert(pos.x == test.configuredPos.x);
  assert(pos.y == test.configuredPos.y);
  assert(size.width == test.configuredSize.width);
  assert(size.height == test.configuredSize.height);

#if defined(_WIN32) || defined(__APPLE__)
  /* Some window managers on Linux (particularly tiling ones) just disregard
     these hints entirely, so we only check that the size is in bounds on MacOS
     and Windows where this is more or less universally supported. */

  assert(size.width >= minSize);
  assert(size.height >= minSize);
  assert(size.width <= maxSize);
  assert(size.height <= maxSize);
#endif

#ifdef _WIN32
  // A configure callback alone is insufficient: suppressing default processing
  // of WM_WINDOWPOSCHANGED also suppresses WM_SIZE, leaving the WGL drawable at
  // its old dimensions even though GetClientRect and Pugl report the new size.
  const HWND window = (HWND)puglGetNativeView(test.view);
  originalWindowProc = (WNDPROC)SetWindowLongPtr(
    window, GWLP_WNDPROC, (LONG_PTR)observeWindowSize);
  assert(originalWindowProc);
  for (unsigned i = 0; i < 2; ++i) {
    const unsigned before = nativeSizeEvents;
    assert(SetWindowPos(window, NULL, 0, 0, i ? 256 : 384, i ? 256 : 384,
                        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE));
    if (nativeSizeEvents == before) {
      fprintf(stderr, "FAIL: resize %u suppressed native WM_SIZE\n", i);
      SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)originalWindowProc);
      puglFreeView(test.view);
      puglFreeWorld(test.world);
      return 1;
    }
    RECT client;
    assert(GetClientRect(window, &client));
    assert(test.configuredSize.width == (PuglSpan)(client.right - client.left));
    assert(test.configuredSize.height == (PuglSpan)(client.bottom - client.top));
  }
  SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)originalWindowProc);
  fprintf(stdout, "PASS: native WM_SIZE delivered for growth and shrink\n");
#endif

  // Tear down
  puglFreeView(test.view);
  puglFreeWorld(test.world);

  return 0;
}
