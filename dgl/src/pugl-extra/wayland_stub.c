// Copyright 2012-2021 David Robillard <d@drobilla.net>
// Copyright 2025 DISTRHO Plugin Framework contributors
// SPDX-License-Identifier: ISC

#include "pugl/stub.h"

#include "../pugl-upstream/src/stub.h"
#include "../pugl-upstream/src/types.h"
#include "wayland.h"

#include "pugl/pugl.h"

const PuglBackend*
puglStubBackend(void)
{
  static const PuglBackend backend = {
    puglStubConfigure,
    puglStubCreate,
    puglStubDestroy,
    puglStubEnter,
    puglStubLeave,
    puglStubGetContext,
  };

  return &backend;
}
