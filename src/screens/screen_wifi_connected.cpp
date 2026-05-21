#include "screen_wifi_connected.h"
#include "../ui/components.h"
#include "../ui/design_tokens.h"
#include <cmath>

using namespace egt;
using namespace std;

// Drop-shadow card: white rounded rectangle with a soft blurred shadow
// behind it. Models the Figma Group 7 effect on the Continue button:
// DROP_SHADOW offset (0,0), radius 10, color rgba(0,0,0,0.10).
// Cairo / EGT has no native gaussian blur, so we fake the blur with a
// stack of concentric rounded rectangles at progressively lower alpha,
// then draw the white filled rect on top.
class ShadowedCard : public Widget {
public:
    ShadowedCard(const Rect& rect,
                 float corner_radius,
                 std::function<void()> on_click)
        : Widget(rect)
        , m_radius(corner_radius)
        , m_on_click(std::move(on_click))
    {
        fill_flags({Theme::FillFlag::blend});
        border(0);
        if (m_on_click) {
            on_event([this](Event& e) {
                if (e.id() == EventId::pointer_click) m_on_click();
            });
        }
    }

    void draw(Painter& painter, const Rect& /*rect*/) override
    {
        auto b = box();
        const float r = m_radius;
        const float x = static_cast<float>(b.x());
        const float y = static_cast<float>(b.y());
        const float w = static_cast<float>(b.width());
        const float h = static_cast<float>(b.height());

        // Soft shadow: 4 layered rounded rects, each 2 px wider, alpha tapers
        // from 25/255 (innermost, ~10 %) down to 6/255 (outermost). With
        // SHADOW_OFF == 0 and rect-grow on each side we get a centred glow.
        constexpr int   layers       = 4;
        constexpr float alpha_inner  = 25.0f;
        constexpr float alpha_outer  = 6.0f;
        for (int i = layers; i >= 1; --i)
        {
            float grow  = static_cast<float>(i) * 2.0f;
            float alpha = alpha_outer +
                          (alpha_inner - alpha_outer) *
                          (1.0f - static_cast<float>(i - 1) / (layers - 1));
            draw_rounded_path(painter, x - grow, y - grow,
                              w + 2.0f * grow, h + 2.0f * grow,
                              r + grow * 0.5f);
            painter.set(Color(0, 0, 0, static_cast<uint8_t>(alpha)));
            painter.fill();
        }

        // White card on top.
        draw_rounded_path(painter, x, y, w, h, r);
        painter.set(dt::kWhite);
        painter.fill();
    }

private:
    static void draw_rounded_path(Painter& p, float x, float y,
                                  float w, float h, float r)
    {
        const float PI = static_cast<float>(M_PI);
        p.draw(PointF(x + r,         y));
        p.line(PointF(x + w - r,     y));
        p.draw(Arc(PointF(x + w - r, y + r),       r, -PI / 2, 0.0f));
        p.line(PointF(x + w,         y + h - r));
        p.draw(Arc(PointF(x + w - r, y + h - r),   r, 0.0f,    PI / 2));
        p.line(PointF(x + r,         y + h));
        p.draw(Arc(PointF(x + r,     y + h - r),   r, PI / 2,  PI));
        p.line(PointF(x,             y + r));
        p.draw(Arc(PointF(x + r,     y + r),       r, PI,    3 * PI / 2));
    }

    float m_radius;
    std::function<void()> m_on_click;
};

shared_ptr<Widget> create_wifi_connected_screen(
    function<void()> on_continue)
{
    auto container = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
    container->fill_flags({Theme::FillFlag::blend});
    container->color(Palette::ColorId::bg, dt::kBgWhite);

    // Figma v5 Group 266: every dimension below = Figma raw * dt::SCALE (1.852).

    // Logo top-left: Figma 90x56 at (4,9) -> 167x104 at (7,17).
    auto logo = ui::create_logo(7, 17, 167, 104);
    container->add(logo);

    // Check circle: Figma 35x35 at (127,86) -> 65x65 at (235,159). Exported
    // from Figma node 2065:1047 (Group 175 = ellipse + check vector) as a
    // 4x PNG; loaded pre-scaled so the natural size matches the target rect.
    constexpr int check_sz = 65;
    try {
        const float check_scale = static_cast<float>(check_sz) / 148.0f;  // PNG 148x148
        auto check_img = Image("file:assets/figma/images/check-circle-green.png",
                               check_scale, check_scale);
        auto check = make_shared<ImageLabel>(check_img);
        check->autoresize(false);
        check->border(0); check->padding(0); check->margin(0);
        check->fill_flags({Theme::FillFlag::blend});
        check->image_align(AlignFlag::center);
        check->box(Rect(235, 159, check_sz, check_sz));
        container->add(check);
    } catch (const std::exception& e) {
        printf("[WIFI_CONNECTED] check icon asset missing: %s\n", e.what());
        fflush(stdout);
    }

    // F1:1 invariant 2: every Label gets border(0) + padding(0) + margin(0)
    // so its content_area equals its box exactly. EGT's theme default border
    // is 2 px, which silently offsets the visible text inward.

    // Title "Wi-Fi Connected": Figma 162x25 at (172,96) -> 300x46 at (319,178),
    // Gothic A1 Bold 20pt -> 37pt scaled. F1:1 invariant 2 (border 0, etc.)
    // applied so the visible glyphs start at exactly x=319. Autoresize stays
    // ON for text labels so the box can grow rightward if cairo renders the
    // text slightly wider than Figma's engine - prevents clipping.
    auto title = make_shared<Label>("Wi-Fi Connected",
        Rect(319, 178, 300, 46));
    title->border(0); title->padding(0); title->margin(0);
    title->font(Font("Gothic A1", 37, Font::Weight::bold));
    title->color(Palette::ColorId::label_text, dt::kGreen);
    title->text_align(AlignFlag::left | AlignFlag::center_vertical);
    container->add(title);

    // Subtitle (two lines): Figma 267x36 at (90,140) -> 494x67 at (167,259),
    // Gothic A1 Regular 14pt -> 26pt scaled.
    auto sub = make_shared<Label>(
        "WiFi connection established successfully.\nThe device is ready to use.",
        Rect(167, 259, 494, 67));
    sub->border(0); sub->padding(0); sub->margin(0);
    sub->font(Font("Gothic A1", 26, Font::Weight::normal));
    sub->color(Palette::ColorId::label_text, dt::kTextPrimary);
    sub->text_align(AlignFlag::center);
    container->add(sub);

    // Continue button: Figma 110x33 at (163,192) -> 204x61 at (302,355).
    // The button + chevron live inside a wrapper Frame so they share the
    // same local coordinate system. This sidesteps the ~30 px y-offset that
    // appears when an ImageLabel is positioned directly inside the screen
    // container alongside a Button at the same absolute coords.
    // Figma node 2065:1063 is a BOOLEAN_OPERATION "Subtract" whose visible
    // bounding box is 8.68 x 12.99, NOT the 13 x 21 of the two source
    // polygons (2065:1064/1065). Using the polygon size made the chevron
    // ~30 % too big. Scale the actual visible chevron: 9*1.852 x 13*1.852.
    constexpr int chev_w = 17;
    constexpr int chev_h = 24;
    const Rect btn_rect(302, 355, 204, 61);

    auto btn_wrap = make_shared<Frame>(btn_rect);
    btn_wrap->fill_flags({});                                   // transparent
    container->add(btn_wrap);

    // Figma Group 7 has NO stroke - the button outline visual is a soft
    // drop shadow with offset (0,0), radius 10, rgba(0,0,0,0.10). Build a
    // ShadowedCard that draws shadow + white rounded rect together so we
    // match Figma instead of the harsh 2 px gray border the default
    // outlined-button helper used. Corner radius 4 in Figma -> 7 scaled.
    constexpr float card_radius = 7.0f;
    auto btn = make_shared<ShadowedCard>(
        Rect(0, 0, btn_rect.width(), btn_rect.height()),
        card_radius,
        std::move(on_continue));
    btn_wrap->add(btn);

    // "Continue" label: Figma TEXT 2065:1061. Per the Figma node:
    //   characters       = "Continue "   (note trailing space)
    //   textAlignHorizontal = CENTER     (NOT left)
    //   bbox 83x18 at button-local (4,9) -> 154x33 at (7,17) scaled
    //   font Gothic A1 Bold 14pt -> 26pt scaled
    // Centring the trailing-space literal in the 154 bbox is what gives
    // Figma's visible "Continue" its small left bias and matches the gap
    // to the chevron.
    auto cont_lbl = make_shared<Label>("Continue ", Rect(7, 17, 154, 33));
    cont_lbl->border(0); cont_lbl->padding(0); cont_lbl->margin(0);
    cont_lbl->font(Font("Gothic A1", 26, Font::Weight::bold));
    cont_lbl->color(Palette::ColorId::label_text, dt::kTextPrimary);
    cont_lbl->text_align(AlignFlag::center);
    btn_wrap->add(cont_lbl);

    // ImageHolder::do_set_image() auto-resizes the widget to the image's
    // natural pixel size and Widget::autoresize defaults to true, which can
    // re-grow the box during a later layout() pass. Two-step protection:
    //   1. Pre-scale the Image at load time via Image(uri, hscale, vscale)
    //      so its natural size already matches the target rect.
    //   2. Set autoresize(false) so any future layout pass cannot grow it.
    // See docs/08-references/egt-widget-reference.md ("ImageLabel" section).
    try {
        const float hscale = static_cast<float>(chev_w) / 35.0f;  // PNG src 35x52
        const float vscale = static_cast<float>(chev_h) / 52.0f;
        auto chev_img = Image("file:assets/figma/images/chevron-right.png", hscale, vscale);
        auto chevron = make_shared<ImageLabel>(chev_img);
        chevron->autoresize(false);
        chevron->border(0); chevron->padding(0); chevron->margin(0);
        chevron->fill_flags({Theme::FillFlag::blend});
        chevron->image_align(AlignFlag::center);  // no `expand`
        // Wrap-local coords (parent Frame is at btn_rect, so 0,0 is its origin).
        // Position in WRAP-LOCAL coords. Figma node 2065:1063 sits at
        // (85.86, 10.64) inside the 110 x 33 Continue button -> (159, 20)
        // inside our 204 x 61 wrap, exactly per dt::SCALE.
        chevron->box(Rect(159, 20, chev_w, chev_h));
        btn_wrap->add(chevron);
    } catch (const std::exception& e) {
        printf("[CONTINUE] chevron asset missing: %s\n", e.what());
        fflush(stdout);
    }

    return container;
}
