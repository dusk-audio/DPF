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

#include "DuskWidgets.hpp"

// Angle-bracketed so a consumer that adds this directory with -isystem does not have to
// take Dear ImGui's own warnings with the widgets.
#include <DearImGui/imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace DuskWidgets {

// --------------------------------------------------------------------------------------------------------------------

static constexpr float kRotaryStart = -2.35619449f; // -135 degrees
static constexpr float kRotaryEnd = 2.35619449f;

// The dome's shading, as JUCE's Colour::brighter / ::darker amounts. Both the baked
// dome and the vector fallback derive their gradient from these two numbers, so the
// two paths cannot drift apart.
static constexpr float kDomeBrighten = 0.20f;
static constexpr float kDomeDarken = 0.40f;
// Where the light sits, in dome radii from the centre.
static constexpr float kDomeHighlightX = -0.45f;
static constexpr float kDomeHighlightY = -0.50f;
static constexpr float kDomeBodyRatio = 0.94f;   // dome radius as a fraction of the well
static constexpr float kDomeTickRadius = 1.027f; // pointer ticks, in dome radii
static constexpr float kDomeShadowRadius = 1.28f;

static float clamp01(float v) noexcept
{
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

// Exact inequality, spelled so a consumer building with -Wfloat-equal does not have to
// turn it off for the whole widget set.
static bool differs(const float a, const float b) noexcept
{
    return std::fabs(a - b) > 0.0f;
}

static bool isUnity(const float v) noexcept
{
    return ! differs(v, 1.0f);
}

static int channel(ImU32 colour, int shift) noexcept
{
    return (int) ((colour >> shift) & 0xff);
}

ImU32 withAlpha(const ImU32 colour, const float alpha) noexcept
{
    const int a = (int) (channel(colour, IM_COL32_A_SHIFT) * clamp01(alpha));
    return (colour & ~((ImU32) 0xff << IM_COL32_A_SHIFT)) | ((ImU32) a << IM_COL32_A_SHIFT);
}

ImU32 brighter(const ImU32 colour, const float amount) noexcept
{
    const float k = 1.0f / (1.0f + amount);
    const auto ch = [k](const int c) { return 255 - (int) ((255 - c) * k); };
    return IM_COL32(ch(channel(colour, IM_COL32_R_SHIFT)), ch(channel(colour, IM_COL32_G_SHIFT)),
                    ch(channel(colour, IM_COL32_B_SHIFT)), channel(colour, IM_COL32_A_SHIFT));
}

ImU32 darker(const ImU32 colour, const float amount) noexcept
{
    const float k = 1.0f / (1.0f + amount);
    const auto ch = [k](const int c) { return (int) (c * k); };
    return IM_COL32(ch(channel(colour, IM_COL32_R_SHIFT)), ch(channel(colour, IM_COL32_G_SHIFT)),
                    ch(channel(colour, IM_COL32_B_SHIFT)), channel(colour, IM_COL32_A_SHIFT));
}

ImU32 lerpColour(const ImU32 a, const ImU32 b, const float t) noexcept
{
    const auto ch = [a, b, t](const int shift) {
        const float x = (float) channel(a, shift);
        const float y = (float) channel(b, shift);
        return (int) (x + (y - x) * clamp01(t));
    };
    return IM_COL32(ch(IM_COL32_R_SHIFT), ch(IM_COL32_G_SHIFT), ch(IM_COL32_B_SHIFT),
                    ch(IM_COL32_A_SHIFT));
}

// --------------------------------------------------------------------------------------------------------------------

Range Range::withMidPoint(const float s, const float e, const float valueAtCentre) noexcept
{
    Range r { s, e, 1.0f };
    const float t = (valueAtCentre - s) / (e - s);
    if (t > 0.0f && t < 1.0f)
        r.skew = std::log(0.5f) / std::log(t);
    return r;
}

// clamp01 cannot stop a NaN - both of its comparisons are false for one - and a NaN
// position reaches the vertex buffer, where it is undefined at rasterisation. A range
// whose ends meet divides by zero, which a host that publishes placeholder parameter
// metadata hands over routinely, and a skew at or below zero sends pow out of [0,1].
// Anything that is not a drawable fraction becomes the bottom of the range here.
static float asFraction(const float v) noexcept
{
    return (v >= 0.0f && v <= 1.0f) ? v : 0.0f;
}

float Range::toNorm(const float value) const noexcept
{
    const float span = end - start;
    const float raw = (span < 0.0f || span > 0.0f) ? (value - start) / span : 0.0f;
    const float c = clamp01(raw);
    return asFraction(isUnity(skew) ? c : std::pow(c, skew));
}

float Range::fromNorm(const float norm) const noexcept
{
    const float c = clamp01(norm);
    const float t = isUnity(skew) ? c : std::pow(c, 1.0f / skew);
    return start + (end - start) * asFraction(t);
}

static const Range& defaultFaderRange()
{
    static const Range r = Range::withMidPoint(-90.0f, 6.0f, -12.0f);
    return r;
}

// --------------------------------------------------------------------------------------------------------------------

void MeterBallistics::tick(float incomingDb, const float framesPerSourceTick) noexcept
{
    // A meter is fed by someone else's DSP, and one bad sample would latch for the life
    // of the UI: NaN fails the rising test below, so the decay folds it into displayed
    // for good, and an infinite peak never falls back. It reads as the signal stopping
    // rather than as a fault, because the bar clamps a NaN to silence.
    if (!(incomingDb > -1000.0f && incomingDb < 1000.0f))
        incomingDb = -100.0f;

    const float frames = framesPerSourceTick > 0.1f ? framesPerSourceTick : 1.0f;
    const float decay = 1.0f - std::pow(1.0f - 0.15f, 1.0f / frames);

    if (incomingDb > displayed)
        displayed = incomingDb;
    else
        displayed += (incomingDb - displayed) * decay;

    if (incomingDb >= peakHold)
    {
        peakHold = incomingDb;
        peakHoldFrames = (int) (18.0f * frames);
    }
    else if (peakHoldFrames > 0)
    {
        --peakHoldFrames;
    }
    else
    {
        peakHold = std::max(-100.0f, peakHold - 1.5f / frames);
    }
}

void MeterBallistics::reset() noexcept
{
    displayed = -100.0f;
    peakHold = -100.0f;
    peakHoldFrames = 0;
}

// --------------------------------------------------------------------------------------------------------------------

const ImWchar* consoleGlyphRanges() noexcept
{
    // Named rather than whole blocks: a face costs glyphs it bakes whether or not
    // anything draws them, and the drawing faces only ever show the marks below. A
    // view that needs another one adds it here, where every face picks it up.
    static const ImWchar ranges[] = {
        0x0020, 0x00ff, // Latin-1, which carries the phase and degree marks
        0x2013, 0x2014, // en and em dash
        0x2018, 0x201d, // typographic quotes
        0x2022, 0x2022, // bullet
        0x2026, 0x2026, // ellipsis
        0x2190, 0x2193, // arrows
        0x221e, 0x221e, // infinity, the fader's floor mark
        0x2264, 0x2265, // not greater / not less
        0x25b2, 0x25c4, // triangles, for transport and disclosure marks
        0x266d, 0x266f, // flat, natural and sharp, for chord names
        0
    };
    return ranges;
}

const ImWchar* textEntryGlyphRanges() noexcept
{
    static const ImWchar ranges[] = {
        0x0020, 0x024f, // Latin and its supplements
        0x0370, 0x052f, // Greek and Cyrillic
        0x0590, 0x06ff, // Hebrew and Arabic
        0x0900, 0x097f, // Devanagari
        0x2000, 0x22ff, // punctuation, arrows, maths
        0x20a0, 0x20cf, // currency
        0x2500, 0x25ff, // box drawing and geometric shapes
        0x3000, 0x30ff, // CJK punctuation and kana, where the face provides them
        0xff00, 0xffef, // full-width forms
        0
    };
    return ranges;
}

Fonts buildFonts(ImFontAtlas& atlas, const void* const ttfData, const int ttfDataSize,
                 const float scale, const FontSizes& sizes)
{
    Fonts fonts;
    if (ttfData == nullptr || ttfDataSize <= 0)
        return fonts;

    ImFontConfig config;
    config.FontDataOwnedByAtlas = false;
    config.OversampleH = 2;
    config.OversampleV = 2;
    config.PixelSnapH = false;

    const auto add = [&](const float size, const ImWchar* const ranges) {
        return atlas.AddFontFromMemoryTTF(const_cast<void*>(ttfData), ttfDataSize,
                                          size * scale, &config, ranges);
    };

    const ImWchar* const console = consoleGlyphRanges();
    fonts.caption = add(sizes.caption, console);
    fonts.label = add(sizes.label, console);
    fonts.pill = add(sizes.pill, console);
    fonts.band = add(sizes.band, console);
    fonts.title = add(sizes.title, console);
    fonts.value = add(sizes.value, console);
    fonts.valueLarge = add(sizes.valueLarge, console);
    fonts.textEntry = add(sizes.textEntry, textEntryGlyphRanges());
    return fonts;
}

// --------------------------------------------------------------------------------------------------------------------
// the baked dome

// The largest dome parameter whose ring still covers this point. The rings march from
// the rim towards the highlight, each one smaller than the last, so the visible colour
// at a point is the one belonging to the last ring that reached it.
static float domeParameter(const float x, const float y) noexcept
{
    constexpr int kSteps = 192;
    for (int i = kSteps; i >= 0; --i)
    {
        const float t = (float) i / (float) kSteps;
        const float dx = x - kDomeHighlightX * t;
        const float dy = y - kDomeHighlightY * t;
        const float ringRadius = 1.0f - t * 0.92f;
        if (dx * dx + dy * dy <= ringRadius * ringRadius)
            return t;
    }
    return 0.0f;
}

static float coverage(const float distance, const float edge, const float texel) noexcept
{
    return clamp01(0.5f + (edge - distance) / std::max(1.0e-6f, texel));
}

void KnobAtlas::reserve(ImFontAtlas& atlas, const int pixels)
{
    size = std::max(16, pixels);
    source = nullptr;
    for (int layer = 0; layer < layerCount; ++layer)
        rects[layer] = atlas.AddCustomRectRegular(size, size);
}

void KnobAtlas::rasterise(ImFontAtlas& atlas)
{
    if (rects[0] < 0)
        return;

    unsigned char* pixels = nullptr;
    int width = 0, height = 0;
    atlas.GetTexDataAsRGBA32(&pixels, &width, &height);
    if (pixels == nullptr)
        return;

    // The dome's own radius maps to this many texels; the rest of the square is the
    // margin the drop shadow and the pointer ticks live in.
    const float perUnit = (float) size * 0.5f / KnobAtlas::kExtent;
    const float texel = 1.0f / perUnit;

    const float kb = 1.0f / (1.0f + kDomeBrighten);
    const float kd = 1.0f / (1.0f + kDomeDarken);
    const float whiteWeight = 1.0f - kb;

    for (int layer = 0; layer < layerCount; ++layer)
    {
        ImFontAtlasCustomRect* const rect = atlas.GetCustomRectByIndex(rects[layer]);
        if (rect == nullptr || !rect->IsPacked())
            continue;

        atlas.CalcCustomRectUV(rect, &uv[layer][0], &uv[layer][1]);

        for (int py = 0; py < size; ++py)
        {
            unsigned int* row = (unsigned int*) pixels
                + (std::size_t) (rect->Y + py) * (std::size_t) width + (std::size_t) rect->X;

            for (int px = 0; px < size; ++px)
            {
                const float x = ((float) px + 0.5f - (float) size * 0.5f) / perUnit;
                const float y = ((float) py + 0.5f - (float) size * 0.5f) / perUnit;
                const float d = std::sqrt(x * x + y * y);
                const float inBody = coverage(d, 1.0f, texel);
                ImU32 out = 0;

                if (layer == body)
                {
                    // Everything that is not the dome itself is black, and black
                    // survives being multiplied by the knob's colour: the drop shadow
                    // and the rim can share the layer that carries the tint.
                    const float shadowY = y - 0.25f;
                    const float shadow = coverage(std::sqrt(x * x + shadowY * shadowY),
                                                  kDomeShadowRadius, texel)
                                       * 0.157f;
                    float alpha = shadow;
                    float shade = 0.0f;

                    if (inBody > 0.0f)
                    {
                        const float t = domeParameter(x, y);
                        const float m = kd + t * (kb - kd);
                        shade = m / std::max(0.05f, 1.0f - t * whiteWeight);
                        // The dark rim the vector dome strokes at the dome's edge.
                        const float rim = clamp01((d - (1.0f - 0.045f)) / 0.045f);
                        shade = shade * (1.0f - rim) + 0.045f * rim;
                        alpha = inBody + shadow * (1.0f - inBody);
                    }

                    const int level = (int) (clamp01(shade) * 255.0f + 0.5f);
                    out = IM_COL32(level, level, level, (int) (clamp01(alpha) * 255.0f + 0.5f));
                }
                else if (layer == sheen)
                {
                    float alpha = 0.0f;
                    if (inBody > 0.0f)
                    {
                        alpha = domeParameter(x, y) * whiteWeight;
                        // The specular smear across the top of the dome.
                        const float ex = x / 0.70f;
                        const float ey = (y + 0.60f) / 0.27f;
                        if (ex * ex + ey * ey <= 1.0f)
                            alpha = 1.0f - (1.0f - alpha) * (1.0f - 36.0f / 255.0f);
                        alpha *= inBody;
                    }
                    out = IM_COL32(255, 255, 255, (int) (clamp01(alpha) * 255.0f + 0.5f));
                }
                else
                {
                    float alpha = 0.0f;
                    for (int i = 0; i < 11; ++i)
                    {
                        const float angle = kRotaryStart
                                          + (kRotaryEnd - kRotaryStart) * ((float) i / 10.0f);
                        const float dx = x - std::sin(angle) * kDomeTickRadius;
                        const float dy = y + std::cos(angle) * kDomeTickRadius;
                        alpha = std::max(alpha,
                                         coverage(std::sqrt(dx * dx + dy * dy), 0.085f, texel));
                    }
                    out = IM_COL32(255, 255, 255, (int) (clamp01(alpha) * 255.0f + 0.5f));
                }

                row[px] = out;
            }
        }
    }

    source = &atlas;
}

bool KnobAtlas::ready() const noexcept
{
    return source != nullptr && source->TexID != 0;
}

ImTextureID KnobAtlas::textureId() const noexcept
{
    return source != nullptr ? source->TexID : (ImTextureID) 0;
}

// --------------------------------------------------------------------------------------------------------------------

bool shortcutsAvailable(const Context& ctx)
{
    return !ctx.textFieldOpen
        && !ImGui::IsAnyItemActive()
        && !ImGui::GetIO().WantTextInput
        && ImGui::GetTopMostPopupModal() == nullptr;
}

void text(const Context& ctx, ImFont* const font, const float size, const ImVec2 at,
          const float width, const ImU32 colour, const char* const str, const Align align)
{
    if (font == nullptr || str == nullptr || *str == 0 || ctx.dl == nullptr)
        return;

    const ImVec2 extent = font->CalcTextSizeA(size, FLT_MAX, 0.0f, str);
    float x = at.x;
    if (align == Align::centre)
        x = at.x + (width - extent.x) * 0.5f;
    else if (align == Align::right)
        x = at.x + width - extent.x;

    ctx.dl->AddText(font, size, ImVec2(std::round(x), std::round(at.y)), colour, str);
}

void panel(const Context& ctx, const ImVec2 tl, const ImVec2 br, const ImU32 fill,
           const ImU32 border, const float borderAlpha, const float rounding,
           const float thickness)
{
    ctx.dl->AddRectFilled(tl, br, fill, rounding);
    ctx.dl->AddRect(tl, br, withAlpha(border, borderAlpha), rounding, 0, thickness);
}

static void drawVectorDome(const Context& ctx, const ImVec2 centre, const float bodyRadius,
                           const ImU32 fill)
{
    const Theme& theme = *ctx.theme;

    for (int i = 0; i < 11; ++i)
    {
        const float t = kRotaryStart + (kRotaryEnd - kRotaryStart) * ((float) i / 10.0f);
        const float radius = bodyRadius * kDomeTickRadius;
        const ImVec2 p(centre.x + std::sin(t) * radius, centre.y - std::cos(t) * radius);
        ctx.dl->AddCircleFilled(p, std::max(0.9f, bodyRadius * 0.064f), theme.knobTick, 8);
    }

    ctx.dl->AddCircleFilled(ImVec2(centre.x, centre.y + bodyRadius * 0.25f),
                            bodyRadius * kDomeShadowRadius, IM_COL32(0, 0, 0, 40), 24);

    const ImU32 hi = brighter(fill, kDomeBrighten);
    const ImU32 lo = darker(fill, kDomeDarken);
    const ImVec2 highlight(centre.x + bodyRadius * kDomeHighlightX,
                           centre.y + bodyRadius * kDomeHighlightY);

    constexpr int kSteps = 12;
    for (int i = 0; i < kSteps; ++i)
    {
        const float t = (float) i / (float) (kSteps - 1);
        const float ringRadius = bodyRadius * (1.0f - t * 0.92f);
        const ImVec2 c(centre.x + (highlight.x - centre.x) * t,
                       centre.y + (highlight.y - centre.y) * t);
        ctx.dl->AddCircleFilled(c, ringRadius, lerpColour(lo, hi, t), 32);
    }

    ctx.dl->AddEllipseFilled(ImVec2(centre.x, centre.y - bodyRadius * 0.60f),
                             ImVec2(bodyRadius * 0.70f, bodyRadius * 0.27f),
                             IM_COL32(255, 255, 255, 0x24), 0.0f, 20);
    ctx.dl->AddCircle(centre, bodyRadius, IM_COL32(0x0b, 0x0b, 0x0d, 255), 32, 1.0f);
}

void drawKnobDome(const Context& ctx, const ImVec2 centre, const float radius,
                  const float norm, const ImU32 fill)
{
    const float well = radius - ctx.s(2.0f);
    if (well <= 1.0f)
        return;

    const Theme& theme = *ctx.theme;
    const float bodyRadius = well * kDomeBodyRatio;

    if (ctx.knobAtlas != nullptr && ctx.knobAtlas->ready())
    {
        const float extent = bodyRadius * KnobAtlas::kExtent;
        const ImVec2 tl(centre.x - extent, centre.y - extent);
        const ImVec2 br(centre.x + extent, centre.y + extent);
        const ImTextureID texture = ctx.knobAtlas->textureId();

        ctx.dl->AddImage(texture, tl, br, ctx.knobAtlas->uvMin(KnobAtlas::body),
                         ctx.knobAtlas->uvMax(KnobAtlas::body), fill);
        ctx.dl->AddImage(texture, tl, br, ctx.knobAtlas->uvMin(KnobAtlas::sheen),
                         ctx.knobAtlas->uvMax(KnobAtlas::sheen), IM_COL32_WHITE);
        ctx.dl->AddImage(texture, tl, br, ctx.knobAtlas->uvMin(KnobAtlas::ticks),
                         ctx.knobAtlas->uvMax(KnobAtlas::ticks), theme.knobTick);
    }
    else
    {
        drawVectorDome(ctx, centre, bodyRadius, fill);
    }

    const float angle = kRotaryStart + clamp01(norm) * (kRotaryEnd - kRotaryStart);
    const ImVec2 dir(std::sin(angle), -std::cos(angle));
    ctx.dl->AddLine(ImVec2(centre.x + dir.x * bodyRadius * 0.82f,
                           centre.y + dir.y * bodyRadius * 0.82f),
                    ImVec2(centre.x + dir.x * bodyRadius, centre.y + dir.y * bodyRadius),
                    theme.knobPointerShadow, std::max(2.4f, well * 0.13f));
    ctx.dl->AddLine(ImVec2(centre.x + dir.x * bodyRadius * 0.16f,
                           centre.y + dir.y * bodyRadius * 0.16f),
                    ImVec2(centre.x + dir.x * bodyRadius * 0.76f,
                           centre.y + dir.y * bodyRadius * 0.76f),
                    theme.knobPointer, std::max(2.0f, well * 0.10f));
}

// --------------------------------------------------------------------------------------------------------------------

bool hitArea(Context& ctx, const char* const id, const ImVec2 tl, const ImVec2 br)
{
    ++ctx.widgets;
    ImGui::SetCursorScreenPos(tl);
    ImGui::InvisibleButton(id, ImVec2(std::max(1.0f, br.x - tl.x), std::max(1.0f, br.y - tl.y)));
    return ImGui::IsItemHovered();
}

void valueBubble(const Context& ctx, const ImVec2 anchor, const char* const str)
{
    if (str == nullptr || *str == 0)
        return;

    const Theme& theme = *ctx.theme;
    ImFont* const font = ctx.fonts->value != nullptr ? ctx.fonts->value : ImGui::GetFont();
    const float size = ctx.s(11.0f);
    const ImVec2 extent = font->CalcTextSizeA(size, FLT_MAX, 0.0f, str);
    const float padX = ctx.s(5.0f);
    const float padY = ctx.s(3.0f);

    const ImVec2 tl(anchor.x + ctx.s(12.0f), anchor.y - extent.y * 0.5f - padY);
    const ImVec2 br(tl.x + extent.x + padX * 2.0f, tl.y + extent.y + padY * 2.0f);

    // The foreground list, so a strip drawn later cannot cover the readout.
    ImDrawList* const dl = ImGui::GetForegroundDrawList();
    dl->AddRectFilled(tl, br, theme.bubbleFill, ctx.s(3.0f));
    dl->AddRect(tl, br, theme.bubbleBorder, ctx.s(3.0f), 0, ctx.s(0.8f));
    dl->AddText(font, size, ImVec2(std::round(tl.x + padX), std::round(tl.y + padY)),
                theme.bubbleText, str);
}

void drawDragBubble(Context& ctx)
{
    if (ctx.drag == nullptr)
        return;
    if (ctx.drag->bubbleText[0] != 0)
        valueBubble(ctx, ctx.drag->bubbleAt, ctx.drag->bubbleText);
    ctx.drag->bubbleText[0] = 0;
}

KnobResult knob(Context& ctx, const char* const id, const ImVec2 centre, const float radius,
                const float value, const Range& range, const float defaultValue,
                const KnobStyle& style)
{
    const Theme& theme = *ctx.theme;
    KnobResult result;
    result.value = value;

    const ImVec2 tl(centre.x - radius, centre.y - radius);
    const ImVec2 br(centre.x + radius, centre.y + radius);
    result.hovered = hitArea(ctx, id, tl, br);
    const ImGuiID itemId = ImGui::GetItemID();

    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f))
    {
        if (ctx.drag->active != itemId)
        {
            ctx.drag->active = itemId;
            ctx.drag->startValue = value;
        }

        const float travel = ctx.s(style.dragTravel);
        const float fine = ImGui::GetIO().KeyShift ? 0.25f : 1.0f;
        const float dy = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f).y;
        const float norm = range.toNorm(ctx.drag->startValue) - (dy / travel) * fine;
        result.value = range.fromNorm(norm);
        result.dragging = true;
    }
    else if (ctx.drag->active == itemId && !ImGui::IsItemActive())
    {
        ctx.drag->active = 0;
    }

    if (result.hovered && differs(ImGui::GetIO().MouseWheel, 0.0f))
        result.value = range.fromNorm(range.toNorm(result.value)
                                      + ImGui::GetIO().MouseWheel * style.wheelStep);

    if (result.hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        result.value = defaultValue;

    result.changed = differs(result.value, value);

    drawKnobDome(ctx, centre, radius, range.toNorm(result.value),
                 style.fill != 0 ? style.fill : theme.knobFill);

    if (style.outline != 0)
        ctx.dl->AddCircle(centre, radius - ctx.s(1.0f), style.outline, 32, ctx.s(1.4f));

    if (style.caption != nullptr)
        text(ctx, ctx.fonts->label, ctx.s(9.0f),
             ImVec2(centre.x - ctx.s(24.0f), centre.y - radius - ctx.s(11.0f)), ctx.s(48.0f),
             theme.textDim, style.caption);

    if (style.value != nullptr)
    {
        text(ctx, ctx.fonts->label, ctx.s(10.5f),
             ImVec2(centre.x - ctx.s(28.0f), centre.y + radius + ctx.s(1.0f)), ctx.s(56.0f),
             theme.textValue, style.value);

        if (result.dragging && style.bubble)
        {
            std::snprintf(ctx.drag->bubbleText, sizeof(ctx.drag->bubbleText), "%s", style.value);
            ctx.drag->bubbleAt = ImVec2(br.x, centre.y);
        }
    }

    return result;
}

// --------------------------------------------------------------------------------------------------------------------

ButtonResult textButton(Context& ctx, const char* const id, const ImVec2 tl, const ImVec2 br,
                        const char* const label, const bool on, const ButtonStyle& style)
{
    const Theme& theme = *ctx.theme;
    ButtonResult result;
    result.hovered = hitArea(ctx, id, tl, br);
    result.clicked = result.hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Left);
    result.rightClicked = result.hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right);

    const ImU32 offFill = style.offFill != 0 ? style.offFill : theme.buttonOff;
    const ImU32 onFill = style.onFill != 0 ? style.onFill : theme.textBright;
    const float rounding = ctx.s(style.rounding);

    ctx.dl->AddRectFilled(tl, br, on ? onFill : offFill, rounding);
    if (result.hovered)
        ctx.dl->AddRectFilled(tl, br,
                              IM_COL32(255, 255, 255, ImGui::IsItemActive() ? 30 : 16), rounding);
    if (style.border)
        ctx.dl->AddRect(tl, br, withAlpha(theme.knobOutline, 0.55f), rounding, 0, ctx.s(0.8f));

    const float size = ctx.s(style.fontSize);
    text(ctx, style.font != nullptr ? style.font : ctx.fonts->pill, size,
         ImVec2(tl.x, tl.y + (br.y - tl.y - size) * 0.5f - ctx.s(1.0f)), br.x - tl.x,
         on ? (style.onText != 0 ? style.onText : theme.textOn)
            : (style.offText != 0 ? style.offText : theme.textDim),
         label);

    return result;
}

ButtonBankResult buttonBank(Context& ctx, const char* const id, const ImVec2 tl, const ImVec2 br,
                            const char* const* const labels, const int count, const int selected,
                            const ButtonBankStyle& style)
{
    ButtonBankResult result;

    if (labels == nullptr || count <= 0)
        return result;

    const Theme& theme = *ctx.theme;
    ImDrawList* const dl = ctx.dl;

    const ImU32 face = style.face != 0 ? style.face : theme.buttonOff;
    const ImU32 faceActive = style.faceActive != 0 ? style.faceActive : darker(face, 0.35f);
    const ImU32 border = style.border != 0 ? style.border : theme.pillBorder;
    const ImU32 tabColour = style.tab != 0 ? style.tab : theme.grHandle;
    const ImU32 labelColour = style.label != 0 ? style.label : theme.textDim;
    const ImU32 labelActive = style.labelActive != 0 ? style.labelActive : theme.textBright;
    const ImU32 captionColour = style.caption != 0 ? style.caption : theme.textDim;

    ImFont* const font = style.font != nullptr ? style.font
                       : (ctx.fonts != nullptr && ctx.fonts->caption != nullptr ? ctx.fonts->caption
                                                                               : ImGui::GetFont());
    const float fontSize = ctx.s(style.fontSize);
    const bool vertical = style.orientation == BankOrientation::vertical;

    // The label gutter comes off the cross axis before the buttons are divided up, so a
    // bank keeps the box it was given whichever way its labels are placed.
    const float gutter = style.labelSide == BankLabelSide::inside ? 0.0f : ctx.s(style.labelGutter);
    ImVec2 bankTl = tl;
    ImVec2 bankBr = br;

    if (style.labelSide == BankLabelSide::before)
    {
        if (vertical)
            bankTl.x += gutter;
        else
            bankTl.y += gutter;
    }
    else if (style.labelSide == BankLabelSide::after)
    {
        if (vertical)
            bankBr.x -= gutter;
        else
            bankBr.y -= gutter;
    }

    if (bankBr.x - bankTl.x < 2.0f || bankBr.y - bankTl.y < 2.0f)
        return result;

    if (style.captionText != nullptr)
    {
        const float size = ctx.s(style.captionSize);
        text(ctx, font, size, ImVec2(tl.x, tl.y - size - ctx.s(4.0f)), br.x - tl.x, captionColour,
             style.captionText);
    }

    const float gap = ctx.s(style.gap);
    const float axis = vertical ? bankBr.y - bankTl.y : bankBr.x - bankTl.x;
    const float pitch = (axis + gap) / (float) count;
    const float length = std::max(ctx.s(2.0f), pitch - gap);
    const float rounding = ctx.s(style.rounding);

    for (int position = 0; position < count; ++position)
    {
        const int index = style.order != nullptr ? style.order[position] : position;
        if (index < 0 || index >= count)
            continue;

        const float offset = (float) position * pitch;
        const ImVec2 buttonTl = vertical ? ImVec2(bankTl.x, bankTl.y + offset)
                                         : ImVec2(bankTl.x + offset, bankTl.y);
        const ImVec2 buttonBr = vertical ? ImVec2(bankBr.x, buttonTl.y + length)
                                         : ImVec2(buttonTl.x + length, bankBr.y);

        // The id stack rather than a composed string, so a long caller id cannot truncate
        // two buttons down to the same ImGui id.
        ImGui::PushID(index);
        const bool hovered = hitArea(ctx, id != nullptr ? id : "##bank", buttonTl, buttonBr);
        const bool held = hovered && ImGui::IsItemActive();
        ImGui::PopID();

        const bool active = index == selected;

        if (hovered)
            result.hovered = index;
        if (hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            result.clicked = index;
            result.changed = !active;
        }

        const bool down = active && style.activeStyle == BankActiveStyle::pressed;

        if (style.shadow && !down)
            dl->AddRectFilled(ImVec2(buttonTl.x + ctx.s(2.0f), buttonTl.y + ctx.s(3.0f)),
                              ImVec2(buttonBr.x + ctx.s(2.0f), buttonBr.y + ctx.s(3.0f)),
                              IM_COL32(0, 0, 0, 130), rounding);

        ImU32 fill = active ? faceActive : face;
        if (hovered)
            fill = brighter(fill, held ? 0.35f : 0.18f);
        dl->AddRectFilled(buttonTl, buttonBr, fill, rounding);

        // The bevel is what carries "pressed": lit along the top and left when the button
        // stands proud, and along the bottom and right when it is pushed in.
        const ImU32 lit = withAlpha(brighter(fill, 0.55f), 0.75f);
        const ImU32 shade = withAlpha(darker(fill, 0.6f), 0.75f);
        const float bevel = ctx.s(1.0f);
        dl->AddLine(ImVec2(buttonTl.x + bevel, buttonTl.y + bevel),
                    ImVec2(buttonBr.x - bevel, buttonTl.y + bevel), down ? shade : lit, bevel);
        dl->AddLine(ImVec2(buttonTl.x + bevel, buttonTl.y + bevel),
                    ImVec2(buttonTl.x + bevel, buttonBr.y - bevel), down ? shade : lit, bevel);
        dl->AddLine(ImVec2(buttonTl.x + bevel, buttonBr.y - bevel),
                    ImVec2(buttonBr.x - bevel, buttonBr.y - bevel), down ? lit : shade, bevel);
        dl->AddLine(ImVec2(buttonBr.x - bevel, buttonTl.y + bevel),
                    ImVec2(buttonBr.x - bevel, buttonBr.y - bevel), down ? lit : shade, bevel);

        dl->AddRect(buttonTl, buttonBr, border, rounding, 0, ctx.s(0.9f));

        if (active && style.activeStyle == BankActiveStyle::tab)
        {
            // A strip along the leading edge: the left of a column, the top of a row.
            const float thickness = ctx.s(2.5f);
            const ImVec2 stripTl(buttonTl.x + bevel, buttonTl.y + bevel);
            const ImVec2 stripBr = vertical ? ImVec2(stripTl.x + thickness, buttonBr.y - bevel)
                                            : ImVec2(buttonBr.x - bevel, stripTl.y + thickness);
            dl->AddRectFilled(stripTl, stripBr, tabColour);
        }

        const ImU32 ink = active ? labelActive : labelColour;
        const char* const label = labels[index];

        switch (style.labelSide)
        {
        case BankLabelSide::inside:
            text(ctx, font, fontSize,
                 ImVec2(buttonTl.x, buttonTl.y + (buttonBr.y - buttonTl.y - fontSize) * 0.5f),
                 buttonBr.x - buttonTl.x, ink, label);
            break;

        case BankLabelSide::before:
            if (vertical)
                text(ctx, font, fontSize,
                     ImVec2(tl.x, buttonTl.y + (buttonBr.y - buttonTl.y - fontSize) * 0.5f),
                     gutter - ctx.s(style.labelGap), ink, label, Align::right);
            else
                text(ctx, font, fontSize,
                     ImVec2(buttonTl.x, bankTl.y - gutter + ctx.s(style.labelGap)),
                     buttonBr.x - buttonTl.x, ink, label);
            break;

        case BankLabelSide::after:
            if (vertical)
                text(ctx, font, fontSize,
                     ImVec2(bankBr.x + ctx.s(style.labelGap),
                            buttonTl.y + (buttonBr.y - buttonTl.y - fontSize) * 0.5f),
                     gutter - ctx.s(style.labelGap), ink, label, Align::left);
            else
                text(ctx, font, fontSize,
                     ImVec2(buttonTl.x, bankBr.y + ctx.s(style.labelGap)),
                     buttonBr.x - buttonTl.x, ink, label);
            break;
        }
    }

    return result;
}

PillResult modulePill(Context& ctx, const char* const id, const ImVec2 tl, const ImVec2 br,
                      const char* const label, const bool engaged, const ImU32 accent)
{
    const Theme& theme = *ctx.theme;
    const float width = br.x - tl.x;
    const float height = br.y - tl.y;
    const float dividerX = tl.x + width * 0.20f;

    PillResult result;
    result.hovered = hitArea(ctx, id, tl, br);
    result.labelAt = ImVec2(dividerX, br.y);

    if (result.hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        if (ImGui::GetIO().MousePos.x < dividerX)
            result.toggled = true;
        else
            result.labelClicked = true;
    }

    ctx.dl->AddRectFilled(tl, br, withAlpha(theme.pillFill, 0.92f), ctx.s(4.0f));
    ctx.dl->AddRect(tl, br, theme.pillBorder, ctx.s(4.0f), 0, ctx.s(0.9f));
    if (result.hovered)
        ctx.dl->AddRectFilled(tl, br, IM_COL32(255, 255, 255, ImGui::IsItemActive() ? 46 : 23),
                              ctx.s(4.0f));

    ctx.dl->AddLine(ImVec2(dividerX, tl.y + ctx.s(2.0f)), ImVec2(dividerX, br.y - ctx.s(2.0f)),
                    withAlpha(theme.pillDivider, 0.55f), ctx.s(1.0f));

    const float ledRadius = std::min(std::max(std::min(width - ctx.s(8.0f), height - ctx.s(6.0f)),
                                              ctx.s(5.0f)),
                                     ctx.s(9.0f))
                          * 0.5f;
    const ImVec2 ledCentre(tl.x + width * 0.10f, tl.y + height * 0.5f);
    ctx.dl->AddCircleFilled(ledCentre, ledRadius + ctx.s(1.0f), theme.ledRing, 16);
    ctx.dl->AddCircleFilled(ledCentre, ledRadius, engaged ? accent : theme.ledOff, 16);
    if (engaged)
        ctx.dl->AddCircleFilled(ImVec2(ledCentre.x, ledCentre.y - ledRadius * 0.3f),
                                ledRadius * 0.45f, withAlpha(brighter(accent, 0.8f), 0.55f), 12);

    const float size = ctx.s(10.5f);
    text(ctx, ctx.fonts->pill, size, ImVec2(dividerX, tl.y + (height - size) * 0.5f - ctx.s(1.0f)),
         br.x - dividerX, engaged ? theme.textBright : theme.textBypassed, label);

    return result;
}

// --------------------------------------------------------------------------------------------------------------------

void meterBackground(const Context& ctx, const ImVec2 tl, const ImVec2 br,
                     const MeterStyle& style)
{
    const Theme& theme = *ctx.theme;
    const float height = br.y - tl.y;

    ctx.dl->AddRectFilled(tl, br, theme.meterBack, ctx.s(1.5f));

    if (style.segments)
    {
        const int segments = std::min(30, std::max(8, (int) (height / ctx.s(3.5f))));
        for (int i = 1; i < segments; ++i)
        {
            const float y = tl.y + height * ((float) i / (float) segments);
            ctx.dl->AddRectFilled(ImVec2(tl.x + ctx.s(1.0f), y),
                                  ImVec2(br.x - ctx.s(1.0f), y + ctx.s(0.8f)), theme.meterSegment);
        }
    }

    ctx.dl->AddRect(tl, br, theme.meterBorder, ctx.s(1.5f), 0, ctx.s(0.5f));
}

void meterBar(const Context& ctx, const ImVec2 tl, const ImVec2 br, const float db,
              const float peakDb, const MeterStyle& style)
{
    const Theme& theme = *ctx.theme;
    const Range& range = style.scale != nullptr ? *style.scale : defaultFaderRange();
    const float height = br.y - tl.y;

    const auto yForDb = [&](const float value) {
        return br.y - height * range.toNorm(std::min(range.end, std::max(range.start, value)));
    };

    const float fillTop = yForDb(db);
    if (fillTop < br.y - 0.5f)
    {
        const ImU32 tip = db >= style.hotDb ? theme.meterHigh
                        : (db >= style.midDb ? theme.meterMid : theme.meterLow);

        if (style.glow)
            ctx.dl->AddRectFilled(ImVec2(tl.x - ctx.s(1.5f), fillTop - ctx.s(1.5f)),
                                  ImVec2(br.x + ctx.s(1.5f), br.y + ctx.s(1.5f)),
                                  withAlpha(tip, 0.20f), ctx.s(2.0f));

        const float hotTop = yForDb(style.hotDb);
        const float midTop = yForDb(style.midDb);
        const auto band = [&](const float top, const float bottom, const ImU32 colour) {
            const float a = std::max(top, fillTop);
            if (a < bottom - 0.25f)
                ctx.dl->AddRectFilled(ImVec2(tl.x, a), ImVec2(br.x, bottom), colour);
        };

        band(tl.y, hotTop, theme.meterHigh);
        band(hotTop, midTop, theme.meterMid);
        band(midTop, br.y, theme.meterLow);
    }

    if (peakDb > range.start + 1.0f)
    {
        const float y = yForDb(peakDb);
        ctx.dl->AddRectFilled(ImVec2(tl.x, y - ctx.s(0.7f)), ImVec2(br.x, y + ctx.s(0.7f)),
                              peakDb >= style.hotDb ? theme.peakTickHot : theme.peakTick);
    }
}

void meter(const Context& ctx, const ImVec2 tl, const ImVec2 br, const float db,
           const float peakDb, const MeterStyle& style)
{
    meterBackground(ctx, tl, br, style);
    meterBar(ctx, tl, br, db, peakDb, style);
}

GainReductionResult gainReduction(Context& ctx, const char* const id, const ImVec2 tl,
                                  const ImVec2 br, const float reductionDb,
                                  const float threshold, const GainReductionStyle& style)
{
    const Theme& theme = *ctx.theme;
    GainReductionResult result;
    result.threshold = threshold;

    const float height = br.y - tl.y;
    const float barRight = style.handle ? br.x - ctx.s(style.handleWidth) : br.x;

    ctx.dl->AddRectFilled(tl, ImVec2(barRight, br.y), theme.grBack, ctx.s(2.0f));

    const float fraction = clamp01(-reductionDb / std::max(1.0e-3f, -style.floorDb));
    if (fraction > 0.001f)
    {
        const float bottom = tl.y + height * fraction;
        const int segments = std::max(1, style.segments);
        for (int i = 0; i < segments; ++i)
        {
            const float a = tl.y + height * ((float) i / (float) segments);
            const float b = tl.y + height * ((float) (i + 1) / (float) segments) - ctx.s(0.6f);
            if (a >= bottom)
                break;
            ctx.dl->AddRectFilled(ImVec2(tl.x + ctx.s(1.0f), a),
                                  ImVec2(barRight - ctx.s(1.0f), std::min(b, bottom)),
                                  lerpColour(theme.grLow, theme.grHigh,
                                             (float) i / (float) std::max(1, segments - 1)));
        }
    }

    ctx.dl->AddRect(tl, ImVec2(barRight, br.y), theme.meterBorder, ctx.s(2.0f), 0, ctx.s(0.5f));

    if (!style.handle)
        return result;

    const Range& range = style.scale != nullptr ? *style.scale : defaultFaderRange();
    const float y = br.y - height * range.toNorm(threshold);
    const bool hovered = hitArea(ctx, id, ImVec2(barRight, y - ctx.s(7.0f)),
                                 ImVec2(br.x, y + ctx.s(7.0f)));

    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f))
    {
        const float norm = clamp01((br.y - ImGui::GetIO().MousePos.y) / height);
        result.threshold = std::min(style.handleMaxDb,
                                    std::max(style.handleMinDb, range.fromNorm(norm)));
        result.changed = differs(result.threshold, threshold);
    }

    const float width = ctx.s(10.0f);
    ctx.dl->AddTriangleFilled(ImVec2(br.x, y - width * 0.6f), ImVec2(br.x, y + width * 0.6f),
                              ImVec2(barRight + ctx.s(1.0f), y),
                              hovered ? brighter(theme.grHandle, 0.4f) : theme.grHandle);
    return result;
}

// --------------------------------------------------------------------------------------------------------------------
// the analogue needle meter

float needleDeflection(const NeedleScale& scale, const float value) noexcept
{
    if (scale.customLaw != nullptr)
        return clamp01(scale.customLaw(value, scale));

    if (scale.law == NeedleLaw::amplitude)
        return clamp01(std::pow(10.0f, (value - scale.maxValue) / 20.0f));

    const float span = scale.maxValue - scale.minValue;
    if (std::fabs(span) < 1.0e-9f)
        return 0.0f;

    return clamp01((value - scale.minValue) / span);
}

void NeedleBallistics::tick(const float targetDeflection, const float deltaSeconds,
                            const float tauSeconds) noexcept
{
    const float target = clamp01(targetDeflection);

    if (deltaSeconds <= 0.0f || tauSeconds <= 0.0f)
    {
        deflection = target;
        return;
    }

    deflection += (target - deflection) * (1.0f - std::exp(-deltaSeconds / tauSeconds));
}

void NeedleBallistics::reset() noexcept
{
    deflection = 0.0f;
}

const NeedleScale& broadcastVuScale() noexcept
{
    // The printed marks of a standard VU face. -2 and +2 carry no number on a real face,
    // which is what `major` says here.
    static const NeedleTick ticks[] = {
        { -20.0f, "20", true },
        { -10.0f, "10", true },
        {  -7.0f,  "7", true },
        {  -5.0f,  "5", true },
        {  -3.0f,  "3", true },
        {  -2.0f, nullptr, false },
        {  -1.0f,  "1", true },
        {   0.0f,  "0", true },
        {   1.0f,  "1", true },
        {   2.0f, nullptr, false },
        {   3.0f,  "3", true },
    };

    static const NeedleScale scale = [] {
        NeedleScale s;
        s.ticks = ticks;
        s.tickCount = (int) (sizeof(ticks) / sizeof(ticks[0]));
        s.minValue = -20.0f;
        // +3 VU is full deflection, so the amplitude law puts 0 VU at 10^(-3/20) = 0.708
        // of the travel, which is where a VU face prints it.
        s.maxValue = 3.0f;
        s.redFrom = 0.0f;
        s.red = true;
        s.law = NeedleLaw::amplitude;
        return s;
    }();

    return scale;
}

const NeedleInnerTick* broadcastVuPercentTicks(int& count) noexcept
{
    // 0 VU reads 100%, and the percentage row is linear in amplitude, so it lands on the
    // same travel as the decibel row scaled by the deflection 0 VU sits at.
    static constexpr float atZeroVu = 0.70794578f; // 10^(-3/20)
    static const NeedleInnerTick ticks[] = {
        { 0.0f * atZeroVu, "0" },
        { 0.2f * atZeroVu, "20" },
        { 0.4f * atZeroVu, "40" },
        { 0.6f * atZeroVu, "60" },
        { 0.8f * atZeroVu, "80" },
        { 1.0f * atZeroVu, "100" },
    };

    count = (int) (sizeof(ticks) / sizeof(ticks[0]));
    return ticks;
}

const NeedleScale& gainReductionScale() noexcept
{
    static const NeedleTick ticks[] = {
        { 20.0f, "20", true },
        { 19.0f, nullptr, false },
        { 18.0f, nullptr, false },
        { 17.0f, nullptr, false },
        { 16.0f, nullptr, false },
        { 15.0f, "15", true },
        { 14.0f, nullptr, false },
        { 13.0f, nullptr, false },
        { 12.0f, nullptr, false },
        { 11.0f, nullptr, false },
        { 10.0f, "10", true },
        {  9.0f, nullptr, false },
        {  8.0f, nullptr, false },
        {  7.0f, nullptr, false },
        {  6.0f, nullptr, false },
        {  5.0f,  "5", true },
        {  4.0f, nullptr, false },
        {  3.0f, nullptr, false },
        {  2.0f, nullptr, false },
        {  1.0f, nullptr, false },
        {  0.0f,  "0", true },
    };

    static const NeedleScale scale = [] {
        NeedleScale s;
        s.ticks = ticks;
        s.tickCount = (int) (sizeof(ticks) / sizeof(ticks[0]));
        // Inverted endpoints: no reduction rests the needle at full right, and it swings
        // left as the compressor works.
        s.minValue = 20.0f;
        s.maxValue = 0.0f;
        s.law = NeedleLaw::linear;
        return s;
    }();

    return scale;
}

void needleMeter(const Context& ctx, const ImVec2 tl, const ImVec2 br, const float deflection,
                 const NeedleScale& scale, const NeedleMeterStyle& style)
{
    ImDrawList* const dl = ctx.dl;

    const ImU32 face = style.face != 0 ? style.face : IM_COL32(0xf0, 0xe7, 0xcd, 0xff);
    const ImU32 faceShade = style.faceShade != 0 ? style.faceShade : darker(face, 0.12f);
    const ImU32 ink = style.ink != 0 ? style.ink : IM_COL32(0x26, 0x20, 0x18, 0xff);
    const ImU32 accent = style.accent != 0 ? style.accent : IM_COL32(0xc4, 0x2a, 0x22, 0xff);
    const ImU32 needleColour = style.needle != 0 ? style.needle : ink;
    const ImU32 innerInk = style.innerInk != 0 ? style.innerInk : withAlpha(ink, 0.62f);

    // The housing, when one was asked for; the face keeps whatever is left.
    ImVec2 faceTl = tl;
    ImVec2 faceBr = br;

    if (style.bezel != 0)
    {
        const float inset = ctx.s(style.bezelInset);
        dl->AddRectFilled(tl, br, style.bezel, ctx.s(style.rounding + 3.0f));
        dl->AddRect(tl, br, darker(style.bezel, 0.45f), ctx.s(style.rounding + 3.0f), 0,
                    ctx.s(1.2f));
        // Two rails, light along the top and dark along the bottom, are enough to read as
        // a machined housing without the four-quad bevel each faceplate had of its own.
        dl->AddLine(ImVec2(tl.x + inset, tl.y + inset * 0.5f),
                    ImVec2(br.x - inset, tl.y + inset * 0.5f),
                    withAlpha(brighter(style.bezel, 0.45f), 0.75f), ctx.s(1.4f));
        dl->AddLine(ImVec2(tl.x + inset, br.y - inset * 0.5f),
                    ImVec2(br.x - inset, br.y - inset * 0.5f),
                    withAlpha(darker(style.bezel, 0.55f), 0.75f), ctx.s(1.4f));

        faceTl = ImVec2(tl.x + inset, tl.y + inset);
        faceBr = ImVec2(br.x - inset, br.y - inset);
    }

    if (faceBr.x - faceTl.x < 4.0f || faceBr.y - faceTl.y < 4.0f)
        return;

    const float rounding = ctx.s(style.rounding);
    dl->AddRectFilled(faceTl, faceBr, face, rounding);
    dl->AddRectFilledMultiColor(faceTl, faceBr, withAlpha(face, 0.0f), withAlpha(face, 0.0f),
                                withAlpha(faceShade, 0.85f), withAlpha(faceShade, 0.85f));
    dl->AddRect(faceTl, faceBr, withAlpha(darker(face, 0.55f), 0.7f), rounding, 0, ctx.s(0.9f));

    // The pivot sits just below the face so the swing fills it, which is how a real meter
    // is built and why the numbers arc rather than sitting on a line.
    const float faceWidth = faceBr.x - faceTl.x;
    const float faceHeight = faceBr.y - faceTl.y;
    const ImVec2 pivot(faceTl.x + faceWidth * 0.5f, faceBr.y - ctx.s(5.0f));
    const float radius = std::min(faceWidth * 0.48f, faceHeight * 0.94f);

    const float startAngle = style.startAngle * 0.01745329f;
    const float endAngle = style.endAngle * 0.01745329f;
    const auto angleFor = [&](const float d) {
        return startAngle + clamp01(d) * (endAngle - startAngle);
    };
    // Angles are measured from straight up so a face reads the way it is described.
    const auto pointAt = [&](const float r, const float angle) {
        return ImVec2(pivot.x + std::sin(angle) * r, pivot.y - std::cos(angle) * r);
    };

    ImFont* const font = style.font != nullptr ? style.font
                       : (ctx.fonts != nullptr && ctx.fonts->caption != nullptr ? ctx.fonts->caption
                                                                               : ImGui::GetFont());

    const auto number = [&](const float r, const float angle, const char* const label,
                            const ImU32 colour, const float size) {
        if (label == nullptr || *label == 0 || font == nullptr)
            return;
        const ImVec2 extent = font->CalcTextSizeA(size, FLT_MAX, 0.0f, label);
        const ImVec2 at = pointAt(r, angle);
        dl->AddText(font, size, ImVec2(std::round(at.x - extent.x * 0.5f),
                                       std::round(at.y - extent.y * 0.5f)), colour, label);
    };

    const float arcRadius = radius * 0.90f;

    if (style.arc)
    {
        // PathArcTo measures from the positive x axis, so the same angle is a quarter turn
        // back from the one the ticks use.
        dl->PathArcTo(pivot, arcRadius, startAngle - 1.5707963f, endAngle - 1.5707963f, 40);
        dl->PathStroke(withAlpha(ink, 0.55f), 0, ctx.s(0.9f));
    }

    if (scale.red)
    {
        const float redStart = needleDeflection(scale, scale.redFrom);
        if (redStart < 0.999f)
        {
            dl->PathArcTo(pivot, arcRadius, angleFor(redStart) - 1.5707963f, endAngle - 1.5707963f,
                          20);
            dl->PathStroke(accent, 0, ctx.s(2.6f));
        }
    }

    const float labelSize = ctx.s(style.labelSize);

    for (int i = 0; i < scale.tickCount; ++i)
    {
        const NeedleTick& tick = scale.ticks[i];
        const float d = needleDeflection(scale, tick.value);
        const float angle = angleFor(d);
        const bool hot = scale.red && ((scale.maxValue >= scale.minValue) ? tick.value > scale.redFrom
                                                                         : tick.value < scale.redFrom);
        const ImU32 colour = hot ? accent : ink;

        dl->AddLine(pointAt(arcRadius, angle),
                    pointAt(arcRadius + ctx.s(tick.major ? 6.0f : 4.0f), angle),
                    colour, ctx.s(tick.major ? 1.5f : 0.9f));

        number(radius * 0.79f, angle, tick.label, colour, labelSize);
    }

    const float innerSize = ctx.s(style.innerLabelSize);
    for (int i = 0; i < scale.innerTickCount; ++i)
        number(radius * 0.59f, angleFor(scale.innerTicks[i].deflection),
               scale.innerTicks[i].label, innerInk, innerSize);

    const float legendSize = ctx.s(style.legendSize);
    if (style.legend != nullptr)
        text(ctx, font, legendSize,
             ImVec2(faceTl.x, pivot.y - radius * 0.46f - legendSize * 0.5f), faceWidth, ink,
             style.legend);
    if (style.sublegend != nullptr)
        text(ctx, font, innerSize,
             ImVec2(faceTl.x, pivot.y - radius * 0.46f + legendSize * 0.6f), faceWidth, innerInk,
             style.sublegend);
    if (style.leftMark != nullptr)
        text(ctx, font, legendSize, ImVec2(faceTl.x + ctx.s(6.0f), faceTl.y + ctx.s(2.0f)),
             ctx.s(16.0f), ink, style.leftMark);
    if (style.rightMark != nullptr)
        text(ctx, font, legendSize,
             ImVec2(faceBr.x - ctx.s(22.0f), faceTl.y + ctx.s(2.0f)), ctx.s(16.0f), accent,
             style.rightMark);

    // The needle: a shadow offset down-right, then a tapered blade, then the mound.
    const float angle = angleFor(deflection);
    const float bladeLength = radius * 0.94f;
    const ImVec2 tip = pointAt(bladeLength, angle);

    dl->AddLine(ImVec2(pivot.x + ctx.s(2.0f), pivot.y + ctx.s(1.2f)),
                ImVec2(tip.x + ctx.s(2.0f), tip.y + ctx.s(1.2f)),
                withAlpha(ink, 0.28f), ctx.s(3.6f));

    const float halfWidth = ctx.s(1.7f);
    const float across = angle + 1.5707963f;
    dl->AddTriangleFilled(ImVec2(pivot.x + std::sin(across) * halfWidth,
                                 pivot.y - std::cos(across) * halfWidth),
                          tip,
                          ImVec2(pivot.x - std::sin(across) * halfWidth,
                                 pivot.y + std::cos(across) * halfWidth),
                          needleColour);

    dl->AddCircleFilled(ImVec2(pivot.x, pivot.y + ctx.s(1.0f)), ctx.s(8.0f),
                        withAlpha(ink, 0.35f), 20);
    dl->AddCircleFilled(pivot, ctx.s(4.6f), needleColour, 18);
    dl->AddCircleFilled(ImVec2(pivot.x - ctx.s(1.2f), pivot.y - ctx.s(1.4f)), ctx.s(1.5f),
                        withAlpha(brighter(face, 0.4f), 0.6f), 10);

    if (style.glass)
        dl->AddRectFilledMultiColor(ImVec2(faceTl.x + ctx.s(3.0f), faceTl.y + ctx.s(2.0f)),
                                    ImVec2(faceBr.x - ctx.s(3.0f),
                                           faceTl.y + faceHeight * 0.24f),
                                    IM_COL32(255, 255, 255, 34), IM_COL32(255, 255, 255, 34),
                                    IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 0));
}

// --------------------------------------------------------------------------------------------------------------------

const FaderTick* decibelFaderTicks(int& count) noexcept
{
    static const FaderTick ticks[] = {
        { 6.0f, "+6" }, { 3.0f, "+3" }, { 0.0f, "0" }, { -3.0f, "3" }, { -6.0f, "6" },
        { -12.0f, "12" }, { -24.0f, "24" }, { -40.0f, "40" }, { -90.0f, "\xe2\x88\x9e" }
    };
    count = (int) (sizeof(ticks) / sizeof(ticks[0]));
    return ticks;
}

FaderResult fader(Context& ctx, const char* const id, const ImVec2 tl, const ImVec2 br,
                  const float value, const Range& range, const float defaultValue,
                  const FaderStyle& style)
{
    const Theme& theme = *ctx.theme;
    FaderResult result;
    result.value = value;

    const float inset = ctx.s(style.trackInset);
    const float trackTop = tl.y + inset;
    const float trackBottom = br.y - inset;
    const float trackHeight = std::max(1.0f, trackBottom - trackTop);
    const float centreX = (tl.x + br.x) * 0.5f;

    hitArea(ctx, id, tl, br);
    if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        const float norm = clamp01((trackBottom - ImGui::GetIO().MousePos.y) / trackHeight);
        result.value = range.fromNorm(norm);
        result.dragging = true;
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        result.value = defaultValue;
    result.changed = differs(result.value, value);

    const float position = trackBottom - trackHeight * range.toNorm(result.value);
    const float capWidth = std::min(br.x - tl.x - ctx.s(6.0f), ctx.s(style.capWidth));
    const float capHeight = ctx.s(style.capHeight);
    const float trackWidth = std::min(ctx.s(4.0f), (br.x - tl.x) * 0.18f);

    ctx.dl->AddRectFilled(ImVec2(centreX - trackWidth * 0.5f, trackTop),
                          ImVec2(centreX + trackWidth * 0.5f, trackBottom), theme.trackFill,
                          trackWidth * 0.5f);
    ctx.dl->AddRectFilledMultiColor(ImVec2(centreX - trackWidth * 0.5f, trackTop),
                                    ImVec2(centreX + trackWidth * 0.5f, trackTop + ctx.s(6.0f)),
                                    IM_COL32(0, 0, 0, 128), IM_COL32(0, 0, 0, 128),
                                    IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0));
    ctx.dl->AddRect(ImVec2(centreX - trackWidth * 0.5f, trackTop),
                    ImVec2(centreX + trackWidth * 0.5f, trackBottom), theme.trackBorder,
                    trackWidth * 0.5f, 0, ctx.s(0.6f));

    int tickCount = style.tickCount;
    const FaderTick* ticks = style.ticks;
    if (ticks == nullptr)
        ticks = decibelFaderTicks(tickCount);

    for (int i = 0; i < tickCount; ++i)
    {
        const float y = trackBottom - trackHeight * range.toNorm(ticks[i].value);
        const bool isZero = ! differs(ticks[i].value, 0.0f);
        const float leftOver = isZero ? ctx.s(4.0f) : ctx.s(3.0f);
        const float rightOver = isZero ? ctx.s(16.0f) : ctx.s(12.0f);

        ctx.dl->AddLine(ImVec2(centreX - trackWidth * 0.5f - leftOver, y),
                        ImVec2(centreX + trackWidth * 0.5f + rightOver, y),
                        IM_COL32(255, 255, 255, isZero ? 0x90 : 0x40),
                        isZero ? ctx.s(1.2f) : ctx.s(0.7f));

        if (!style.gutter || ticks[i].label == nullptr)
            continue;

        // The lowest tick is the infinity mark, which reads too small at the size the
        // numbers are set in.
        const bool isFloor = i == tickCount - 1;
        const float size = ctx.s(isFloor ? 16.0f : 10.5f);
        text(ctx, ctx.fonts->band, size,
             ImVec2(style.gutterLeft, y - size * (isFloor ? 0.75f : 0.55f)),
             centreX - capWidth * 0.5f - style.gutterLeft - ctx.s(3.0f),
             isZero ? theme.textBright : theme.tickLabel, ticks[i].label, Align::right);
    }

    const ImVec2 capTL(centreX - capWidth * 0.5f, position - capHeight * 0.5f);
    const ImVec2 capBR(centreX + capWidth * 0.5f, position + capHeight * 0.5f);

    ctx.dl->AddRectFilled(ImVec2(capTL.x - ctx.s(2.0f), capTL.y + ctx.s(2.0f)),
                          ImVec2(capBR.x + ctx.s(2.0f), capBR.y + ctx.s(4.0f)),
                          IM_COL32(0, 0, 0, 140), ctx.s(4.0f));

    const float bands[] = { 0.0f, 0.30f, 0.50f, 0.70f, 1.0f };
    const ImU32 stops[] = { theme.capTop, theme.capUpper, theme.capMid, theme.capUpper,
                            theme.capBottom };
    for (int i = 0; i < 4; ++i)
        ctx.dl->AddRectFilledMultiColor(ImVec2(capTL.x, capTL.y + capHeight * bands[i]),
                                        ImVec2(capBR.x, capTL.y + capHeight * bands[i + 1]),
                                        stops[i], stops[i], stops[i + 1], stops[i + 1]);

    ctx.dl->AddLine(ImVec2(capTL.x, capTL.y + ctx.s(1.0f)),
                    ImVec2(capBR.x, capTL.y + ctx.s(1.0f)), IM_COL32(255, 255, 255, 0x70),
                    ctx.s(1.0f));
    ctx.dl->AddLine(ImVec2(capTL.x, capBR.y - ctx.s(2.0f)),
                    ImVec2(capBR.x, capBR.y - ctx.s(2.0f)), IM_COL32(0, 0, 0, 0x40), ctx.s(1.0f));
    ctx.dl->AddRect(capTL, capBR, theme.capRim, ctx.s(2.5f), 0, ctx.s(1.0f));

    for (int i = -1; i <= 1; ++i)
    {
        const float y = position + ctx.s(4.0f) * (float) i;
        ctx.dl->AddRectFilled(ImVec2(capTL.x + ctx.s(3.0f), y),
                              ImVec2(capBR.x - ctx.s(3.0f), y + ctx.s(1.6f)), theme.capGroove);
        ctx.dl->AddLine(ImVec2(capTL.x + ctx.s(3.0f), y + ctx.s(2.4f)),
                        ImVec2(capBR.x - ctx.s(3.0f), y + ctx.s(2.4f)),
                        IM_COL32(255, 255, 255, 0x35), ctx.s(0.9f));
    }

    if (result.dragging && style.bubble)
    {
        if (style.valueText != nullptr)
            std::snprintf(ctx.drag->bubbleText, sizeof(ctx.drag->bubbleText), "%s",
                          style.valueText);
        else
            formatDecibels(ctx.drag->bubbleText, sizeof(ctx.drag->bubbleText), result.value);
        ctx.drag->bubbleAt = ImVec2(capBR.x, position);
    }

    return result;
}

// --------------------------------------------------------------------------------------------------------------------

TextFieldResult textField(Context& ctx, const char* const id, const ImVec2 tl, const ImVec2 br,
                          char* const buffer, const std::size_t bufferSize, const bool takeFocus)
{
    ++ctx.widgets;
    ctx.textFieldOpen = true;

    const Theme& theme = *ctx.theme;
    TextFieldResult result;

    ImGui::SetCursorScreenPos(tl);
    ImGui::PushItemWidth(br.x - tl.x);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::ColorConvertU32ToFloat4(theme.fieldFill));
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(theme.fieldText));
    if (ctx.fonts->textEntry != nullptr)
        ImGui::PushFont(ctx.fonts->textEntry);

    if (takeFocus)
        ImGui::SetKeyboardFocusHere();

    const bool entered = ImGui::InputText(id, buffer, bufferSize,
                                          ImGuiInputTextFlags_EnterReturnsTrue
                                              | ImGuiInputTextFlags_AutoSelectAll);
    result.active = ImGui::IsItemActive();
    if (entered)
    {
        result.committed = true;
    }
    else if (ImGui::IsItemDeactivated())
    {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            result.cancelled = true;
        else
            result.committed = true;
    }

    if (ctx.fonts->textEntry != nullptr)
        ImGui::PopFont();
    ImGui::PopStyleColor(2);
    ImGui::PopItemWidth();
    return result;
}

// --------------------------------------------------------------------------------------------------------------------

void formatFrequency(char* const out, const std::size_t size, const float hz)
{
    if (hz >= 1000.0f)
    {
        const float k = hz / 1000.0f;
        if (k >= 10.0f)
            std::snprintf(out, size, "%.0fk", k);
        else
            std::snprintf(out, size, "%.1fk", k);
    }
    else
    {
        std::snprintf(out, size, "%.0f", hz);
    }
}

void formatGain(char* const out, const std::size_t size, const float db)
{
    if (std::fabs(db - std::round(db)) < 0.05f)
        std::snprintf(out, size, "%+.0f", db);
    else
        std::snprintf(out, size, "%+.1f", db);
}

void formatDecibels(char* const out, const std::size_t size, const float db,
                    const float minusInfinityAt)
{
    if (db <= minusInfinityAt)
        std::snprintf(out, size, "\xe2\x88\x9e");
    else
        std::snprintf(out, size, "%.1f", db);
}

} // namespace DuskWidgets
