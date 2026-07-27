Khronos OpenGL headers vendored for MSVC
========================================

The Microsoft Visual C++ toolchain ships an <GL/gl.h> that is stuck at OpenGL 1.1 and provides
neither <GL/glext.h> nor <KHR/khrplatform.h>. DGL needs those to build its OpenGL backend, so they
are vendored here instead of being downloaded at configure time (an unpinned network fetch during
`cmake` is neither reproducible nor usable offline).

Only cmake/DPF-plugin.cmake adds this directory to the include path, and only when MSVC is the
compiler. Every other toolchain (MinGW, gcc, clang, Apple) keeps using its own system headers.

Contents
--------

  GL/glext.h        https://registry.khronos.org/OpenGL/api/GL/glext.h
                    GL_GLEXT_VERSION 20260609
                    retrieved 2026-07-26

  KHR/khrplatform.h https://registry.khronos.org/EGL/api/KHR/khrplatform.h
                    retrieved 2026-07-26

Both files are unmodified copies of the upstream Khronos registry versions and carry their own
license headers (glext.h is MIT via SPDX tag, khrplatform.h is the Khronos MIT-style license). To
update, re-download from the URLs above and refresh the retrieval date and version noted here.
