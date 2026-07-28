# DPF - DISTRHO Plugin Framework
[![build](https://github.com/dusk-audio/DPF/actions/workflows/build.yml/badge.svg)](https://github.com/dusk-audio/DPF/actions/workflows/build.yml)
[![cmake](https://github.com/dusk-audio/DPF/actions/workflows/cmake.yml/badge.svg)](https://github.com/dusk-audio/DPF/actions/workflows/cmake.yml)
[![wayland](https://github.com/dusk-audio/DPF/actions/workflows/wayland.yml/badge.svg)](https://github.com/dusk-audio/DPF/actions/workflows/wayland.yml)

This is the [Dusk Audio](https://github.com/dusk-audio) plugin framework, derived from
[DISTRHO/DPF](https://github.com/DISTRHO/DPF) at `4238e1c7` (2025-10-23) and maintained
independently since. It adds native Wayland support, MSVC/arm64 build coverage and a set of
state-handling fixes; see [Differences from DISTRHO/DPF](#differences-from-distrhodpf).

This is a standalone repository, not a GitHub fork, and development is one-way in both directions:
nothing here is submitted to DISTRHO, and no DISTRHO commit is merged back in. Report bugs here,
including in code inherited from DPF.

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


## Differences from DISTRHO/DPF

- **Native Wayland windowing.** A pugl Wayland backend for DGL (`dgl/src/pugl-extra/wayland*`),
  selected when the X11 development files are absent. X11 remains the backend on any machine that
  has it; there is no runtime switch.
- **Wayland plugin UIs.** CLAP advertises `CLAP_WINDOW_API_WAYLAND` as floating-only, and LV2
  declares the generic `ui:UI` class on Wayland builds so conforming hosts take the
  `ui:showInterface` path. X11 builds are unchanged in both cases.
- **CLAP headers 1.2.10** (DISTRHO vendors 1.1.2), keeping one local addition: the `uptr`
  member in `clap_window_t`, marked in the header so a refresh does not drop it.
- **macOS defaults to the core-profile GL3 renderer.** `UI_TYPE` defaults to `opengl3` on macOS
  only, retiring the deprecated legacy 2.1 immediate-mode context there; an explicit `UI_TYPE`
  still wins, and `gles2`/`gles3` are rejected on macOS instead of silently building desktop GL.
- **MSVC support.** `Mutex`/`Signal` and the WASAPI standalone backend build under MSVC.
- **Windows on ARM.** VST3 bundles emit the `arm64-win` binary directory.
- **macOS deployment target defaults to 10.15**, overridable via `MACOSX_DEPLOYMENT_TARGET`. The
  universal build keeps 10.8-compatible sources while targeting 10.15 for the arm64 slice.
- **AU windows follow the host.** The AU wrapper tracks parent view frame and backing scale
  changes, so hosts that resize the plugin view (Logic Pro restoring a saved window size, a display
  change between 1x and 2x) reach the UI; see [UI host-resize](FEATURES.md#au-host-resize).
- **State and preset fixes.** VST3 and CLAP `setState` no longer read one byte past the end of the
  host chunk, CLAP loads state written by a differently configured build of the same plugin
  (parameters or states compiled out), and AU factory preset data is allocated and freed as an
  array.
- **CI covers the shipped surface and nothing else.** `build.yml` (Makefile, macOS arm64 + the
  Ubuntu 22.04 floor), `cmake.yml` (Linux x86_64, native ARM Linux, macOS, MSVC x64) and
  `wayland.yml` (Wayland-only build, X11 regression, clap-validator and pluginval runs with no
  external package repositories). Formats, UI types and architectures that are not shipped are not
  built: no 32-bit anything, no Intel-only macOS legs, no MinGW, no Cairo or GLES UI legs. macOS
  builds universal, so the x86_64 slice is still compiled and shipped alongside arm64; what was
  dropped is the separate Intel-only CI targets, not Intel support.


## Licensing

DPF is released under ISC, which basically means you can do whatever you want as long as you credit the original authors.  
Some plugin formats may have additional restrictions, see [LICENSING.md](LICENSING.md) for details.

This tree is a derivative work of [DISTRHO/DPF](https://github.com/DISTRHO/DPF) by Filipe Coelho
and contributors, used and modified under those same ISC terms; the original copyright notices are
retained in every file that carries them.


## Help and documentation

Bug reports belong on the [dusk-audio/DPF issue tracker](https://github.com/dusk-audio/DPF/issues),
including for code inherited from DPF: this tree is maintained here and nothing reported to
DISTRHO reaches it. If a bug also reproduces on DISTRHO/DPF, reporting it there as well helps their
users, but it is not a substitute.

Online documentation for the core API is at [https://distrho.github.io/DPF/](https://distrho.github.io/DPF/).
It still applies here, since the public plugin and UI API is unchanged; anything under
`distrho/src` or `dgl/src` may have diverged.

DISTRHO's [DPF discussions](https://github.com/DISTRHO/DPF/discussions) cover DPF itself. Questions
about *this* tree belong in this repository's issue tracker instead, since its maintainers do not
have it.


## List of plugins made with DPF:

See [DISTRHO's wiki page](https://github.com/DISTRHO/DPF/wiki/Plugins-made-with-DPF) for a list of plugins made with DPF.

Plugin examples are also available in the `examples/` folder inside this repo.
