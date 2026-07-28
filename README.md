# DPF - DISTRHO Plugin Framework
[![build](https://github.com/dusk-audio/DPF/actions/workflows/build.yml/badge.svg)](https://github.com/dusk-audio/DPF/actions/workflows/build.yml)
[![cmake](https://github.com/dusk-audio/DPF/actions/workflows/cmake.yml/badge.svg)](https://github.com/dusk-audio/DPF/actions/workflows/cmake.yml)
[![wayland](https://github.com/dusk-audio/DPF/actions/workflows/wayland.yml/badge.svg)](https://github.com/dusk-audio/DPF/actions/workflows/wayland.yml)

This is the [Dusk Audio](https://github.com/dusk-audio) fork of
[DISTRHO/DPF](https://github.com/DISTRHO/DPF), maintained as the plugin framework for our own
products. It tracks upstream and adds native Wayland support, MSVC/arm64 build coverage and a
set of state-handling fixes; see [Differences from upstream](#differences-from-upstream).
Fork-specific changes are not submitted upstream, so file issues here rather than with DISTRHO.

DPF is designed to make development of new plugins an easy and enjoyable task.  
It allows developers to create plugins with custom UIs using a simple C++ API.  
The framework facilitates exporting various different plugin formats from the same code-base.

DPF can build for LADSPA, DSSI, LV2, VST2, VST3, CLAP and AU formats.  
A JACK/Standalone mode is also available, allowing you to quickly test plugins.

Plugin DSP and UI communication is done via key-value string pairs.  
You send messages from the UI to the DSP side, which is automatically saved in the host when required.  
(You can also store state internally if needed, but this breaks DSSI compatibility).

Getting time information from the host is possible.  
It uses the same format as the JACK Transport API, making porting some code easier.

Provided features and implementation status for specific plugin formats can be seen in [FEATURES.md](FEATURES.md).

On Linux, DPF builds against X11 by default and against Wayland natively when the X11 development
files are absent; see [Wayland support](FEATURES.md#wayland-support) for what each build can do.
The Wayland backend needs `wayland-client`, `wayland-egl`, `wayland-cursor`, `xkbcommon` and `egl`.
When switching a checkout between backends, set `BUILD_DIR_SUFFIX` per backend and remove the
affected `bin/` bundles, otherwise objects and binaries from the other backend are silently reused.


## Differences from upstream

- **Native Wayland windowing.** A pugl Wayland backend for DGL (`dgl/src/pugl-extra/wayland*`),
  selected when the X11 development files are absent. X11 remains the backend on any machine that
  has it; there is no runtime switch.
- **Wayland plugin UIs.** CLAP advertises `CLAP_WINDOW_API_WAYLAND` as floating-only, and LV2
  declares the generic `ui:UI` class on Wayland builds so conforming hosts take the
  `ui:showInterface` path. X11 builds are unchanged in both cases.
- **CLAP headers 1.2.10** (upstream vendors 1.1.2), keeping one DPF-local addition: the `uptr`
  member in `clap_window_t`, marked in the header so a refresh does not drop it.
- **macOS defaults to the core-profile GL3 renderer.** `UI_TYPE` defaults to `opengl3` on macOS
  only, retiring the deprecated legacy 2.1 immediate-mode context there; an explicit `UI_TYPE`
  still wins, and `gles2`/`gles3` are rejected on macOS instead of silently building desktop GL.
- **MSVC support.** `Mutex`/`Signal` and the WASAPI standalone backend build under MSVC.
- **Windows on ARM.** VST3 bundles emit the `arm64-win` binary directory.
- **macOS deployment target defaults to 10.15**, overridable via `MACOSX_DEPLOYMENT_TARGET`. The
  universal build keeps 10.8-compatible sources while targeting 10.15 for the arm64 slice.
- **State and preset fixes.** VST3 and CLAP `setState` no longer read one byte past the end of the
  host chunk, CLAP accepts the zero-parameter zero-state fast-path stream, and AU factory preset
  data is allocated and freed as an array.
- **CI reworked for this fork's support surface.** 16 checks: `build.yml`, `cmake.yml` (including a
  native ARM Linux runner and three MSVC legs) and `wayland.yml` (Wayland-only, X11 regression,
  clap-validator and pluginval runs with no external package repositories).


## Licensing

DPF is released under ISC, which basically means you can do whatever you want as long as you credit the original authors.  
Some plugin formats may have additional restrictions, see [LICENSING.md](LICENSING.md) for details.


## Help and documentation

Bug reports for this fork happen on the [dusk-audio/DPF issue tracker](https://github.com/dusk-audio/DPF/issues).  
Issues that reproduce on upstream DPF are better reported to the
[DISTRHO/DPF project](https://github.com/DISTRHO/DPF/issues).

Online documentation for the core API is available at [https://distrho.github.io/DPF/](https://distrho.github.io/DPF/)
and applies to this fork, which does not change the public API.

General discussion about DPF happens in the upstream [DPF github discussions](https://github.com/DISTRHO/DPF/discussions).


## List of plugins made with DPF:

See [this upstream wiki page](https://github.com/DISTRHO/DPF/wiki/Plugins-made-with-DPF) for a list of plugins made with DPF.

Plugin examples are also available in the `examples/` folder inside this repo.
