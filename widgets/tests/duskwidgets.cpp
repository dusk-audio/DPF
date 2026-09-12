/*
 * Dusk console widgets for DAF
 * Copyright (C) 2026 Dusk Audio
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

// Every widget in the Dusk set on one window: what they look like, how they behave, and
// a working example of the two calls the baked knob dome needs around atlas build time.

// ImGui is quite large, build it separately
#define IMGUI_SKIP_IMPLEMENTATION

#include "Application.hpp"
#include "../opengl/DearImGui.cpp"
#include "../opengl/DuskWidgets.hpp"
#include "src/Resources.hpp"

#include <cmath>
#include <cstdio>

START_NAMESPACE_DGL

namespace dw = DuskWidgets;

class DuskWidgetsGallery : public ImGuiStandaloneWindow
{
public:
    DuskWidgetsGallery(Application& app)
        : ImGuiStandaloneWindow(app)
    {
        ImGuiIO& io(ImGui::GetIO());
        io.Fonts->Clear();
        fonts = dw::buildFonts(*io.Fonts, daf_resources::dejavusans_ttf,
                               (int)daf_resources::dejavusans_ttf_size, 1.0f);
        // Reserved before the atlas is packed, rasterised after: the dome is three
        // rectangles inside the font texture, so it costs no draw call of its own.
        knobs.reserve(*io.Fonts, 128);
        io.FontDefault = fonts.band;
        io.Fonts->Build();
        knobs.rasterise(*io.Fonts);

        // The stock broadcast face plus its percentage row, which is optional because a
        // gain-reduction face has no second row to print.
        vuScale = dw::broadcastVuScale();
        vuScale.innerTicks = dw::broadcastVuPercentTicks(vuScale.innerTickCount);
    }

protected:
    void onImGuiDisplay() override
    {
        const float width = getWidth();
        const float height = getHeight();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(width, height));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_WindowBg,
                              ImGui::ColorConvertU32ToFloat4(theme.background));
        ImGui::Begin("##gallery", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
                     | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);

        dw::Context ctx;
        ctx.dl = ImGui::GetWindowDrawList();
        ctx.theme = &theme;
        ctx.fonts = &fonts;
        ctx.knobAtlas = &knobs;
        ctx.drag = &drag;
        ctx.scale = 1.0f;

        char text[32];
        const ImU32 colours[] = { IM_COL32(0xc4, 0x44, 0x44, 0xff),
                                  IM_COL32(0x5f, 0xa5, 0x5f, 0xff),
                                  IM_COL32(0x58, 0x78, 0xb0, 0xff),
                                  IM_COL32(0xd0, 0x90, 0x60, 0xff),
                                  IM_COL32(0xe0, 0xe0, 0xe4, 0xff) };
        const char* const captions[] = { "GAIN", "FREQ", "Q", "DRIVE", "MIX" };

        dw::text(ctx, fonts.title, 13.0f, ImVec2(20, 14), 200, theme.textBright,
                 "Dusk widgets", dw::Align::left);

        for (int i = 0; i < 5; ++i)
        {
            char id[8];
            std::snprintf(id, sizeof id, "##k%d", i);
            std::snprintf(text, sizeof text, "%+.1f", knobValues[i]);

            dw::KnobStyle style;
            style.fill = colours[i];
            style.caption = captions[i];
            style.value = text;

            const auto result = dw::knob(ctx, id, ImVec2(46.0f + i * 66.0f, 74.0f), 16.0f,
                                         knobValues[i], gainRange, 0.0f, style);
            if (result.changed)
                knobValues[i] = result.value;
        }

        const bool engaged = compEngaged;
        const auto pill = dw::modulePill(ctx, "##pill", ImVec2(20, 118), ImVec2(200, 138),
                                         "COMP", engaged, colours[3]);
        if (pill.toggled)
            compEngaged = !engaged;
        if (pill.labelClicked)
            ImGui::OpenPopup("##pillMenu");
        if (ImGui::BeginPopup("##pillMenu"))
        {
            ImGui::MenuItem("Opto");
            ImGui::MenuItem("FET");
            ImGui::EndPopup();
        }

        dw::ButtonStyle button;
        button.onFill = IM_COL32(0xff, 0x45, 0x00, 0xff);
        button.fontSize = 11.0f;
        if (dw::textButton(ctx, "##mute", ImVec2(20, 146), ImVec2(78, 168), "MUTE", muted,
                           button)
                .clicked)
            muted = !muted;

        button.onFill = IM_COL32(0xcc, 0xcc, 0x00, 0xff);
        if (dw::textButton(ctx, "##solo", ImVec2(84, 146), ImVec2(142, 168), "SOLO", soloed,
                           button)
                .clicked)
            soloed = !soloed;

        if (editingName)
        {
            const auto field = dw::textField(ctx, "##name", ImVec2(20, 176), ImVec2(200, 198),
                                             name, sizeof name, takeFocus);
            takeFocus = false;
            if (field.committed || field.cancelled)
                editingName = false;
        }
        else
        {
            if (dw::hitArea(ctx, "##nameLabel", ImVec2(20, 176), ImVec2(200, 198))
                && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                editingName = true;
                takeFocus = true;
            }
            dw::text(ctx, fonts.title, 13.0f, ImVec2(20, 180), 180, theme.textValue, name,
                     dw::Align::left);
        }

        dw::text(ctx, fonts.caption, 8.0f, ImVec2(20, 206), 180, theme.textDim,
                 "double-click the name to rename, the knobs to reset", dw::Align::left);

        // A moving programme signal, so the meter and the gain reduction column show
        // their ballistics rather than a still frame.
        phase += ImGui::GetIO().DeltaTime * 1.7f;
        const float beat = std::fmod(phase, 1.0f);
        const float level = -48.0f + 46.0f * std::exp(-beat * 5.0f);
        meterBallistics.tick(level, 2.0f);

        const float top = 240.0f;
        const float bottom = std::max(top + 80.0f, height - 30.0f);

        dw::FaderStyle faderStyle;
        faderStyle.gutterLeft = 20.0f;
        const auto moved = dw::fader(ctx, "##fader", ImVec2(66, top), ImVec2(106, bottom),
                                     faderDb, faderRange, 0.0f, faderStyle);
        if (moved.changed)
            faderDb = moved.value;

        dw::meter(ctx, ImVec2(112, top + 18.0f), ImVec2(126, bottom - 18.0f),
                  meterBallistics.displayed, meterBallistics.peakHold);

        dw::GainReductionStyle grStyle;
        grStyle.handle = true;
        const auto gr = dw::gainReduction(ctx, "##gr", ImVec2(130, top + 18.0f),
                                          ImVec2(154, bottom - 18.0f),
                                          std::min(0.0f, threshold - meterBallistics.displayed),
                                          threshold, grStyle);
        if (gr.changed)
            threshold = gr.threshold;

        // The needle meters, driven from the same programme signal, with the scale as data:
        // the same widget is a broadcast VU or a gain-reduction meter depending only on
        // which NeedleScale it is handed.
        vuNeedle.tick(dw::needleDeflection(vuScale, meterBallistics.displayed + 2.0f),
                      ImGui::GetIO().DeltaTime);
        grNeedle.tick(dw::needleDeflection(dw::gainReductionScale(),
                                           std::max(0.0f, meterBallistics.displayed - threshold)),
                      ImGui::GetIO().DeltaTime);

        const float rightLeft = 180.0f;
        const float rightRight = std::max(rightLeft + 120.0f, width - 20.0f);

        dw::NeedleMeterStyle vuStyle;
        vuStyle.bezel = IM_COL32(0xa8, 0x8c, 0x60, 0xff);
        vuStyle.legend = "VU";
        vuStyle.sublegend = "L";
        vuStyle.leftMark = "-";
        vuStyle.rightMark = "+";
        dw::needleMeter(ctx, ImVec2(rightLeft, top), ImVec2(rightRight, top + 140.0f),
                        vuNeedle.deflection, vuScale, vuStyle);

        dw::NeedleMeterStyle grStyle2;
        grStyle2.bezel = IM_COL32(0x4b, 0x4f, 0x50, 0xff);
        grStyle2.face = IM_COL32(0xf3, 0xd6, 0x8d, 0xff);
        grStyle2.ink = IM_COL32(0x5e, 0x40, 0x22, 0xff);
        grStyle2.legend = "VU";
        grStyle2.sublegend = "GAIN REDUCTION";
        dw::needleMeter(ctx, ImVec2(rightLeft, top + 150.0f),
                        ImVec2(rightRight, top + 290.0f), grNeedle.deflection,
                        dw::gainReductionScale(), grStyle2);

        // A latching bank in each orientation and each active style, including the
        // visual-order remap a printed ratio column needs.
        static const char* const ratios[] = { "4", "8", "12", "20", "ALL" };
        static const int ratioOrder[] = { 3, 2, 1, 0, 4 };

        dw::ButtonBankStyle bank;
        bank.captionText = "RATIO";
        bank.order = ratioOrder;
        const auto ratio = dw::buttonBank(ctx, "##ratio", ImVec2(rightLeft, top + 330.0f),
                                          ImVec2(rightLeft + 58.0f, top + 460.0f), ratios, 5,
                                          ratioIndex, bank);
        if (ratio.changed)
            ratioIndex = ratio.clicked;

        dw::ButtonBankStyle meterBank;
        meterBank.orientation = dw::BankOrientation::horizontal;
        meterBank.labelSide = dw::BankLabelSide::inside;
        meterBank.activeStyle = dw::BankActiveStyle::pressed;
        meterBank.captionText = "METER";
        meterBank.face = IM_COL32(0x9a, 0x9a, 0x9e, 0xff);
        meterBank.label = IM_COL32(0x2a, 0x2a, 0x2e, 0xff);
        meterBank.labelActive = IM_COL32(0x10, 0x10, 0x12, 0xff);
        static const char* const meterModes[] = { "GR", "+8", "+4", "OFF" };
        const auto mode = dw::buttonBank(ctx, "##metermode",
                                         ImVec2(rightLeft + 78.0f, top + 340.0f),
                                         ImVec2(rightRight, top + 372.0f), meterModes, 4,
                                         meterMode, meterBank);
        if (mode.changed)
            meterMode = mode.clicked;

        dw::drawDragBubble(ctx);

        std::snprintf(text, sizeof text, "%d widgets   %d verts", ctx.widgets,
                      ctx.dl->VtxBuffer.Size);
        dw::text(ctx, fonts.value, 11.0f, ImVec2(width - 180.0f, 16.0f), 160, theme.textDim,
                 text, dw::Align::right);

        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }

private:
    dw::Theme theme;
    dw::Fonts fonts;
    dw::KnobAtlas knobs;
    dw::DragState drag;
    dw::MeterBallistics meterBallistics;
    dw::NeedleBallistics vuNeedle;
    dw::NeedleBallistics grNeedle;
    dw::NeedleScale vuScale;
    dw::Range gainRange { -15.0f, 15.0f, 1.0f };
    dw::Range faderRange = dw::Range::withMidPoint(-90.0f, 6.0f, -12.0f);

    float knobValues[5] = { 2.5f, -1.5f, 0.0f, 6.0f, -3.0f };
    float faderDb = 0.0f;
    float threshold = -14.0f;
    float phase = 0.0f;
    int ratioIndex = 1;
    int meterMode = 0;
    char name[64] = "KICK IN";
    bool compEngaged = true;
    bool muted = false;
    bool soloed = false;
    bool editingName = false;
    bool takeFocus = false;
};

END_NAMESPACE_DGL

int main(int, char**)
{
    USE_NAMESPACE_DGL;

    Application app;
    DuskWidgetsGallery win(app);
    win.setGeometryConstraints(460, 480, false);
    win.setSize(520, 780);
    win.setResizable(true);
    win.setTitle("Dusk widgets");
    win.show();
    app.exec();

    return 0;
}
