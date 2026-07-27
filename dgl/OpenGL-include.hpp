/*
 * DISTRHO Plugin Framework (DPF)
 * Copyright (C) 2012-2021 Filipe Coelho <falktx@falktx.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any purpose with
 * or without fee is hereby granted, provided that the above copyright notice and this
 * permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD
 * TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef DGL_OPENGL_INCLUDE_HPP_INCLUDED
#define DGL_OPENGL_INCLUDE_HPP_INCLUDED

#include "../distrho/src/DistrhoDefines.h"

// --------------------------------------------------------------------------------------------------------------------
// Fix OpenGL includes for Windows, based on glfw code (part 1)

#undef DGL_WINGDIAPI_DEFINED

#ifdef DISTRHO_OS_WINDOWS

#ifndef WINAPI
# define WINAPI __stdcall
#endif

#ifndef APIENTRY
# define APIENTRY WINAPI
#endif // APIENTRY

/* We need WINGDIAPI defined */
#ifndef WINGDIAPI
# if defined(_MSC_VER) || defined(__BORLANDC__) || defined(__POCC__)
#  define WINGDIAPI __declspec(dllimport)
# elif defined(__LCC__)
#  define WINGDIAPI __stdcall
# else
#  define WINGDIAPI extern
# endif
# define DGL_WINGDIAPI_DEFINED
#endif // WINGDIAPI

#endif // DISTRHO_OS_WINDOWS

// --------------------------------------------------------------------------------------------------------------------
// OpenGL includes

#ifdef DISTRHO_OS_MAC
// There is no OpenGL ES on macOS: Apple ships no desktop GLES implementation and no <GLES2/gl2.h>, so there is
// nothing to include here for a GLES build. This used to silently #undef DGL_USE_GLES/GLES2/GLES3 and fall
// through to the desktop gl3.h path below, which meant a UI_TYPE=gles2 or gles3 build on macOS quietly produced
// an ordinary desktop GL3 binary -- the build succeeded, the CI leg went green, and nothing GLES was ever
// compiled or tested. Fail loudly instead, so the build says what it cannot do.
# ifdef DGL_USE_GLES
#  error OpenGL ES is not supported on macOS, use UI_TYPE=opengl3 (desktop GL3 core profile) instead
# endif
# ifdef DGL_USE_OPENGL3
#  include <OpenGL/gl3.h>
#  include <OpenGL/gl3ext.h>
# else
#  include <OpenGL/gl.h>
# endif
#else
# ifndef DISTRHO_OS_WINDOWS
#  define GL_GLEXT_PROTOTYPES
# endif
# ifndef __GLEW_H__
#  include <GL/gl.h>
#  include <GL/glext.h>
# endif
#endif

// --------------------------------------------------------------------------------------------------------------------
// Missing OpenGL defines

#if defined(GL_BGR_EXT) && !defined(GL_BGR)
# define GL_BGR GL_BGR_EXT
#endif

#if defined(GL_BGRA_EXT) && !defined(GL_BGRA)
# define GL_BGRA GL_BGRA_EXT
#endif

#ifndef GL_CLAMP_TO_BORDER
# define GL_CLAMP_TO_BORDER 0x812D
#endif

// --------------------------------------------------------------------------------------------------------------------
// Fix OpenGL includes for Windows, based on glfw code (part 2)

#ifdef DGL_WINGDIAPI_DEFINED
# undef WINGDIAPI
# undef DGL_WINGDIAPI_DEFINED
#endif

// --------------------------------------------------------------------------------------------------------------------

#endif
