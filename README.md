# DPF - DISTRHO Plugin Framework
[![build](https://github.com/dusk-audio/DPF/actions/workflows/build.yml/badge.svg)](https://github.com/dusk-audio/DPF/actions/workflows/build.yml)
[![cmake](https://github.com/dusk-audio/DPF/actions/workflows/cmake.yml/badge.svg)](https://github.com/dusk-audio/DPF/actions/workflows/cmake.yml)
[![wayland](https://github.com/dusk-audio/DPF/actions/workflows/wayland.yml/badge.svg)](https://github.com/dusk-audio/DPF/actions/workflows/wayland.yml)

DPF is designed to make development of new plugins an easy and enjoyable task.  
It allows developers to create plugins with custom UIs using a simple C++ API.  
The framework facilitates exporting various different plugin formats from the same code-base.

DPF can build for LADSPA, DSSI, LV2, VST2, VST3 and CLAP formats.  
A JACK/Standalone mode is also available, allowing you to quickly test plugins.

Plugin DSP and UI communication is done via key-value string pairs.  
You send messages from the UI to the DSP side, which is automatically saved in the host when required.  
(You can also store state internally if needed, but this breaks DSSI compatibility).

Getting time information from the host is possible.  
It uses the same format as the JACK Transport API, making porting some code easier.

Provided features and implementation status for specific plugin formats can be seen in [FEATURES.md](FEATURES.md).

On Linux, DPF builds against X11 by default and against Wayland natively when the X11 development
files are absent; see [Wayland support](FEATURES.md#wayland-support) for what each build can do.

## Licensing

DPF is released under ISC, which basically means you can do whatever you want as long as you credit the original authors.  
Some plugin formats may have additional restrictions, see [LICENSING.md](LICENSING.md) for details.


## Help and documentation

Bug reports happen on the [DPF github project](https://github.com/DISTRHO/DPF/issues).

Online documentation is available at [https://distrho.github.io/DPF/](https://distrho.github.io/DPF/).

Online help and discussion about DPF happens in the [DPF github discussions](https://github.com/DISTRHO/DPF/discussions).


## List of plugins made with DPF:

See [this wiki page](https://github.com/DISTRHO/DPF/wiki/Plugins-made-with-DPF) for a list of plugins made with DPF.

Plugin examples are also available in the `example/` folder inside this repo.
