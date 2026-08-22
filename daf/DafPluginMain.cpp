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

#include "src/DafPlugin.cpp"

#if defined(DAF_PLUGIN_TARGET_AU)
# include "src/DafPluginAU.cpp"
#elif defined(DAF_PLUGIN_TARGET_CARLA)
# include "src/DafPluginCarla.cpp"
#elif defined(DAF_PLUGIN_TARGET_CLAP)
# include "src/DafPluginCLAP.cpp"
#elif defined(DAF_PLUGIN_TARGET_JACK)
# include "src/DafPluginJACK.cpp"
#elif (defined(DAF_PLUGIN_TARGET_LADSPA) || defined(DAF_PLUGIN_TARGET_DSSI))
# include "src/DafPluginLADSPA+DSSI.cpp"
#elif defined(DAF_PLUGIN_TARGET_LV2)
# include "src/DafPluginLV2.cpp"
# include "src/DafPluginLV2export.cpp"
#elif defined(DAF_PLUGIN_TARGET_MAPI)
# include "src/DafPluginMAPI.cpp"
#elif defined(DAF_PLUGIN_TARGET_VST2)
# include "src/DafPluginVST2.cpp"
#elif defined(DAF_PLUGIN_TARGET_VST3)
# include "src/DafPluginVST3.cpp"
#elif defined(DAF_PLUGIN_TARGET_EXPORT)
# include "src/DafPluginExport.cpp"
#elif defined(DAF_PLUGIN_TARGET_STATIC)
START_NAMESPACE_DAF
Plugin* createStaticPlugin() { return createPlugin(); }
END_NAMESPACE_DAF
#else
# error unsupported format
#endif

#if defined(DAF_PLUGIN_TARGET_JACK)
# define DAF_IS_STANDALONE 1
#else
# define DAF_IS_STANDALONE 0
#endif
#include "src/DafUtils.cpp"
