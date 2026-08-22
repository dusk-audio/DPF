/*
 * DISTRHO Plugin Framework (DPF)
 * Copyright (C) 2012-2025 Filipe Coelho <falktx@falktx.com>
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

#include "src/DafUI.cpp"

// we might be building a plugin with external UI, which works on most formats except VST2/3
#if ! DAF_PLUGIN_HAS_UI && ! defined(DAF_PLUGIN_VST_HPP_INCLUDED)
# error Trying to build UI without DAF_PLUGIN_HAS_UI set to 1
#endif

#if DAF_PLUGIN_HAS_UI

#if defined(DAF_PLUGIN_TARGET_AU)
# define DAF_PLUGIN_AND_UI_IN_SINGLE_OBJECT 1
# import "src/DafUIAU.mm"
#elif defined(DAF_PLUGIN_TARGET_CARLA)
# define DAF_PLUGIN_AND_UI_IN_SINGLE_OBJECT 1
#elif defined(DAF_PLUGIN_TARGET_CLAP)
# define DAF_PLUGIN_AND_UI_IN_SINGLE_OBJECT 1
#elif defined(DAF_PLUGIN_TARGET_JACK)
# define DAF_PLUGIN_AND_UI_IN_SINGLE_OBJECT 1
#elif defined(DAF_PLUGIN_TARGET_DSSI)
# define DAF_PLUGIN_AND_UI_IN_SINGLE_OBJECT 0
# include "src/DafUIDSSI.cpp"
#elif defined(DAF_PLUGIN_TARGET_LV2)
# if defined(DAF_PLUGIN_AND_UI_IN_SINGLE_OBJECT)
#  if ! DAF_PLUGIN_WANT_DIRECT_ACCESS
#   warning Using single/monolithic LV2 target while DAF_PLUGIN_WANT_DIRECT_ACCESS is 0
#  endif
# else
#  define DAF_PLUGIN_AND_UI_IN_SINGLE_OBJECT DAF_PLUGIN_WANT_DIRECT_ACCESS
# endif
# include "src/DafUILV2.cpp"
#elif defined(DAF_PLUGIN_TARGET_VST2)
# define DAF_PLUGIN_AND_UI_IN_SINGLE_OBJECT 1
#elif defined(DAF_PLUGIN_TARGET_VST3)
# define DAF_PLUGIN_AND_UI_IN_SINGLE_OBJECT 1
# include "src/DafUIVST3.cpp"
#elif defined(DAF_PLUGIN_TARGET_EXPORT)
# define DAF_PLUGIN_AND_UI_IN_SINGLE_OBJECT 1
#elif defined(DAF_PLUGIN_TARGET_SHARED) || defined(DAF_PLUGIN_TARGET_STATIC)
# define DAF_PLUGIN_AND_UI_IN_SINGLE_OBJECT 1
#else
# error unsupported format
#endif

#if !DAF_PLUGIN_AND_UI_IN_SINGLE_OBJECT
# ifdef DAF_PLUGIN_TARGET_DSSI
#  define DAF_IS_STANDALONE 1
# else
#  define DAF_IS_STANDALONE 0
# endif
# include "src/DafUtils.cpp"
#else
# ifdef DAF_PLUGIN_TARGET_JACK
#  define DAF_IS_STANDALONE 1
# else
#  define DAF_IS_STANDALONE 0
# endif
#endif

#if defined(DAF_UI_LINUX_WEBVIEW_START) && !DAF_IS_STANDALONE
int main(int argc, char* argv[])
{
    return DAF_NAMESPACE::daf_webview_start(argc, argv);
}
#elif defined(DAF_OS_LINUX) && defined(DGL_USE_WEB_VIEW) && !DAF_IS_STANDALONE
int main()
{
    return 0;
}
#endif

#endif
