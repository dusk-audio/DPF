# DAF Widgets
## Reusable GUI widgets for [DAF](https://github.com/dusk-audio/DAF), the Dusk Audio Framework

**This is a fork of [DISTRHO/DPF-Widgets](https://github.com/DISTRHO/DPF-Widgets) by Filipe Coelho
and contributors.** Most of the code here is theirs and it is very good. The fork exists so that
[Dusk Audio](https://github.com/dusk-audio) builds pin one tree they control alongside
[DAF](https://github.com/dusk-audio/DAF). If you are building on DPF, use DISTRHO's original.

On top of upstream this tree carries an MSVC compatibility fix, a handful of Dear ImGui
integration fixes (telling ImGui when the window focus changes, releasing the graphics context in
the standalone constructor, keeping non-finite values out of the draw list, and clearing key state
on focus loss without cancelling a drag in progress), and one widget set of its own,
`dusk/DuskWidgets`, described under Status below.

Renamed from DPF-Widgets to DAF-Widgets in August 2026 so that two identically named repositories
stop sending people and tooling to the wrong tree. The name marks whose checkout this is, not
whose work it is. Original copyright notices and licence terms are retained in every file.

Development here is one-way in both directions: nothing is submitted to DISTRHO, and no DISTRHO
commit is merged back in. Report bugs on the
[dusk-audio/DAF issue tracker](https://github.com/dusk-audio/DAF/issues).

The widget kit is vendored inside DAF under `widgets/`.

#### generic / ResizeHandle

Resize handle for DAF windows, will sit on bottom-right.

Works in both Cairo and OpenGL modes (classic/legacy OpenGL only, does not support OpenGL3 mode).

Used very often and in many plugins.


#### imgui / DearImGui

![screenshot](https://raw.githubusercontent.com/DISTRHO/dear-plugins/main/plugins/ImGuiDemo/Screenshot.png)

Exposes the [Dear ImGui](https://github.com/ocornut/imgui/) drawing API inside a DGL Widget.
The drawing function `onDisplay()` is implemented internally but a new `onImGuiDisplay()` needs to be overridden instead.
This class will take care of setting up ImGui for drawing, and also also user input, resizes and everything in between.

Used in:

- [dear-plugins](https://github.com/DISTRHO/dear-plugins)
- [Ildaeil](https://github.com/DISTRHO/Ildaeil)
- [master_me](https://github.com/trummerschlunk/master_me/) (histogram display)
- [WSTD CRSHR](https://github.com/Wasted-Audio/wstd-crshr)
- [WSTD DLAY](https://github.com/Wasted-Audio/wstd-dlay)
- [WSTD DL3Y](https://github.com/Wasted-Audio/wstd-dl3y)
- [WSTD EQ](https://github.com/Wasted-Audio/wstd-eq)
- [WSTD 3Q](https://github.com/Wasted-Audio/wstd-3q)
- [WSTD FLANGR](https://github.com/Wasted-Audio/wstd-flangr)
- [WSTD FL3NGR](https://github.com/Wasted-Audio/wstd-fl3ngr)
- [WSTD FLDR](https://github.com/Wasted-Audio/wstd-fldr)
- [WSTD MANGLR](https://github.com/Wasted-Audio/wstd-manglr)
- [WSTD M3NGLR](https://github.com/Wasted-Audio/wstd-m3nglr)
- [WSTD SMTHR](https://github.com/Wasted-Audio/wstd-smthr)

See [imgui-template-plugin](https://github.com/DISTRHO/imgui-template-plugin/) for a CMake-based template plugin project around ImGui.
See [imgui-template-app](https://github.com/DISTRHO/imgui-template-app/) for a standalone application template.


#### dusk / DuskWidgets

![screenshot](screenshots/DuskWidgets.png)

The mixing-console widget set shared by Dusk Studio and the Dusk plug-in UIs: the knob with its
pre-rendered dome, the full-travel fader, the segmented meter, the gain-reduction column, the
analogue needle meter, the latching button bank, the module header pill, buttons, the drag value
bubble and the text field, plus the theme, the font baking and the shared value formatting.

The set is a namespace of free functions taking
an `ImDrawList` and a `Context`, not a `SubWidget` subclass, so that a host which already has a Dear
ImGui context can draw with it whatever windowing it runs on: Dusk Studio uses it outside DGL
entirely. It depends on Dear ImGui only.

**`widgets/dusk/DuskWidgets.cpp` has to be compiled**, alongside `widgets/imgui/DearImGui.cpp`. Including the
header alone links only for the parts that are inline, which is none of the widgets. A CMake
consumer adds it to the same source list its ImGui backend is in; the gallery under `tests/` is a
worked example of the whole set, including the two calls the baked knob dome needs around
`ImFontAtlas::Build()`.

Values go in and come back out: a widget never writes through a pointer, so the caller decides
whether a parameter lives in an atomic, in a host parameter or in a plain float.

Being adopted by Dusk Studio and the Dusk plug-in fleet, which reached the same widgets
independently and are converging on this copy.


Widgets use one header and an optional implementation file, live in `DGL_NAMESPACE`
(the ImGui-only Dusk kit uses its own namespace), work across platforms, and use
an ISC-compatible licence.
