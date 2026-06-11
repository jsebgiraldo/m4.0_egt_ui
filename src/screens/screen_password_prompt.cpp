#include <egt/ui>
#include <egt/virtualkeyboard.h>
#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <cctype>
#include <cmath>

#include "../ui/design_tokens.h"
#include "../ui/components.h"

using namespace egt;
using namespace std;

// Layout constants
static constexpr int SCREEN_W       = 800;
static constexpr int SCREEN_H       = 480;
static constexpr int CARD_W         = 780;
// 468 (not 460) so keyboard row 4 — figma rows land at y≈417..474 global —
// fits inside the card without clipping (figma's white panel actually runs
// past the 480 px screen edge).
static constexpr int CARD_H         = 468;
static constexpr int PAD            = 15;

// Figma-matched colors
static const Color KEY_BG        = Color(244, 244, 244);     // gradient top approximation
static const Color KEY_SPECIAL   = Color(228, 229, 232);     // special key bg (backspace, Return, shift, .?123)
static const Color KEY_TEXT      = dt::kTextPrimary;          // rgb(100,101,105)
static const Color INPUT_BG      = Color(217, 217, 217, 128);// Figma: gray at 50% opacity
static const Color JOIN_BG       = dt::kAccentCyan;           // rgb(48,163,196) Figma gradient top
static const Color CANCEL_BG     = dt::kWhite;               // Figma: white fill
static const Color CANCEL_FG     = dt::kTextPrimary;         // rgb(100,101,105)

// Rounded-rect path via the portable draw(PointF)/line/Arc idiom — EGT 1.10
// (target) has no Painter::move_to/line_to.
static void rounded_rect_path(Painter& painter, float x, float y,
                              float w, float h, float r)
{
    const auto PI = static_cast<float>(M_PI);
    painter.draw(PointF(x + r, y));
    painter.line(PointF(x + w - r, y));
    painter.draw(Arc(PointF(x + w - r, y + r), r, -PI / 2, 0.0f));
    painter.line(PointF(x + w, y + h - r));
    painter.draw(Arc(PointF(x + w - r, y + h - r), r, 0.0f, PI / 2));
    painter.line(PointF(x + r, y + h));
    painter.draw(Arc(PointF(x + r, y + h - r), r, PI / 2, PI));
    painter.line(PointF(x, y + r));
    painter.draw(Arc(PointF(x + r, y + r), r, PI, 3 * PI / 2));
}

// ── KeyButton — custom-drawn keyboard key ─────────────────────────────────
// Figma 39:2026: no stroke, radius 2 figma (≈4 dev) and a drop shadow of
// 1px 1px 2px rgba(0,0,0,0.2). Letter keys + spacebar fill with a vertical
// #f4f4f4 → #ffffff gradient; special keys (Return, .?123, shift, ⌫, blank)
// are flat #e4e5e8. EGT's themed Button can't do gradient fills or
// borderless drop shadows, so everything is painted in draw() — same
// approach as PressableOperateCard in screen_wifi_not_found.cpp, which also
// keeps press feedback + on-click on the widget itself (no overlay children
// to steal taps on EGT 1.10). The widget box is padded by kShadowPad so the
// shadow isn't clipped at the box edge.
class KeyButton : public Widget {
public:
    static constexpr int kShadowPad = 3;

    KeyButton(const string& label, const Rect& key, bool special,
              const Font& font, function<void()> action)
        : Widget(Rect(key.x() - kShadowPad, key.y() - kShadowPad,
                      key.width() + 2 * kShadowPad,
                      key.height() + 2 * kShadowPad)),
          m_label(label), m_special(special), m_font(font),
          m_action(std::move(action))
    {
        fill_flags({Theme::FillFlag::blend});
        border(0);
        on_event([this](Event& e) {
            switch (e.id()) {
            case EventId::raw_pointer_down:
                if (m_action && !m_pressed) { m_pressed = true;  damage(); }
                break;
            case EventId::raw_pointer_up:
                if (m_pressed)              { m_pressed = false; damage(); }
                break;
            case EventId::pointer_click:
                if (m_action) m_action();
                break;
            default: break;
            }
        });
    }

    void draw(Painter& painter, const Rect&) override
    {
        const auto b = content_area();
        const float x = b.x() + kShadowPad, y = b.y() + kShadowPad;
        const float w = b.width()  - 2.0f * kShadowPad;
        const float h = b.height() - 2.0f * kShadowPad;
        const float r = 4.0f;  // 2 figma px ≈ 3.7 dev

        // Drop shadow 1px 1px 2px rgba(0,0,0,0.2): faint blur halo + tighter
        // core, both offset (1,1). No stroke (figma keys have no border).
        rounded_rect_path(painter, x - 1.0f, y - 1.0f,
                          w + 4.0f, h + 4.0f, r + 2.0f);
        painter.set(Color(0, 0, 0, 13));
        painter.fill();
        rounded_rect_path(painter, x + 1.0f, y + 1.0f, w + 1.0f, h + 1.0f, r);
        painter.set(Color(0, 0, 0, 34));
        painter.fill();

        // Key body.
        rounded_rect_path(painter, x, y, w, h, r);
        if (m_pressed) {
            painter.set(dt::kGreenLight);
        } else if (m_special) {
            painter.set(KEY_SPECIAL);
        } else {
            // Figma 39:2026 — vertical gradient #f4f4f4 → #ffffff.
            Pattern grad(Pattern::StepArray{{0.0f, KEY_BG}, {1.0f, dt::kWhite}},
                         Point(static_cast<int>(x), static_cast<int>(y)),
                         Point(static_cast<int>(x), static_cast<int>(y + h)));
            painter.set(grad);
        }
        painter.fill();

        // Glyph centred on the key.
        if (!m_label.empty()) {
            painter.set(KEY_TEXT);
            painter.set(m_font);
            const auto ts = painter.text_size(m_label);
            painter.draw(PointF(x + (w - ts.width()) / 2.0f,
                                y + (h - ts.height()) / 2.0f));
            painter.draw(m_label);
        }
    }

private:
    string m_label;
    bool m_special{false};
    Font m_font;
    function<void()> m_action;
    bool m_pressed{false};
};

// ── InputInnerShadow — top inner-shadow band for the password field ───────
// Figma 72:1594: inset 0 2px 4px rgba(0,0,0,0.2). EGT has no inner shadows,
// so paint a short band that fades from rgba(0,0,0,~50) to transparent over
// ~11 device px (2px offset + 4px blur figma ≈ 11 dev). readonly => taps
// fall through to the TextBox underneath (same trick as SoftShadow in
// screen_patient_info.cpp — EGT skips readonly widgets in input dispatch).
class InputInnerShadow : public Widget {
public:
    InputInnerShadow(const Rect& band, float radius)
        : Widget(band), m_radius(radius)
    {
        fill_flags({Theme::FillFlag::blend});
        border(0);
        readonly(true);
    }

    void draw(Painter& painter, const Rect&) override
    {
        const auto b = content_area();
        const float x = b.x(), y = b.y(), w = b.width(), h = b.height();
        const float r = m_radius;
        const auto PI = static_cast<float>(M_PI);
        // Band path: top corners rounded (match the field), square bottom.
        painter.draw(PointF(x + r, y));
        painter.line(PointF(x + w - r, y));
        painter.draw(Arc(PointF(x + w - r, y + r), r, -PI / 2, 0.0f));
        painter.line(PointF(x + w, y + h));
        painter.line(PointF(x, y + h));
        painter.line(PointF(x, y + r));
        painter.draw(Arc(PointF(x + r, y + r), r, PI, 3 * PI / 2));
        Pattern grad(Pattern::StepArray{{0.0f, Color(0, 0, 0, 50)},
                                        {1.0f, Color(0, 0, 0, 0)}},
                     Point(static_cast<int>(x), static_cast<int>(y)),
                     Point(static_cast<int>(x), static_cast<int>(y + h)));
        painter.set(grad);
        painter.fill();
    }

private:
    float m_radius;
};

shared_ptr<Widget> create_password_prompt_screen(
    const string& title_text,
    const string& message,
    const string& join_label,
    const string& cancel_label,
    function<void(const string&)> on_join,
    function<void()> on_cancel)
{
    // Figma: clean white background, no dark overlay
    auto main_frame = make_shared<Frame>(Rect(0, 0, SCREEN_W, SCREEN_H));
    main_frame->color(Palette::ColorId::bg, dt::kGrayBg);

    // Card — minimal styling matching Figma Rectangle 9. y is pinned at 10
    // (not re-centred) so the title/input/button positions that already
    // match figma stay put while the extra height extends downward only.
    auto card = make_shared<Frame>(
        Rect((SCREEN_W - CARD_W) / 2, 10, CARD_W, CARD_H));
    card->color(Palette::ColorId::bg, dt::kWhite);
    card->border(0);
    card->border_radius(8);
    main_frame->add(card);

    // ---- INICIO: Layout dinámico superior ----
    int y_cursor = 12; // margen superior mínimo

    // Título — Figma: Gothic A1/12/700, rgb(100,101,105) (scaled to 22px)
    auto title = make_shared<Label>(title_text.empty() ? string("Enter Password") : title_text);
    title->resize(Size(CARD_W - PAD * 2, 34));
    title->move(Point(PAD, y_cursor));
    title->font(Font(22, Font::Weight::bold));
    title->color(Palette::ColorId::label_text, dt::kTextPrimary);
    card->add(title);
    y_cursor += 34; // avanzar debajo del título

    // Subtitle line ("message" param) is intentionally not rendered. Figma
    // node 144:877 has just the title + input row + Forgot Password +
    // keyboard, no per-user context line. The parameter is kept in the
    // signature for compatibility with callers that pass it.
    (void)message;

    y_cursor += 10; // espacio antes del input
    // ---- FIN: Layout dinámico superior ----

    // Layout horizontal: campo de contraseña + botones de acción. Input row
    // spans the full card width so the Cancel + Join PNGs (148x70 / 172x70)
    // fit at their figma-derived global positions without clipping.
    // 80 px tall (not 70) so the 59 px field at y=14 isn't clipped; the
    // Cancel/Join PNG wraps keep their matched y=0 positions.
    auto input_row = make_shared<Frame>(Rect(0, y_cursor, CARD_W, 80));
    input_row->color(Palette::ColorId::bg, Color(0, 0, 0, 0));
    card->add(input_row);

    // Password field - Figma 72:1594: gray fill rgba(217,217,217,.5), r=2,
    // NO border, inner shadow inset 0 2px 4px rgba(0,0,0,0.2). Device rect
    // (28,80) 420x59 (figma (15,353) 227x32 ×1.852) -> input_row-local
    // (18,14) since input_row sits at global (10, 66).
    auto pwd = make_shared<TextBox>("Password..");
    pwd->resize(Size(420, 59));
    pwd->move(Point(18, 14));
    pwd->font(Font("Gothic A1", 22, Font::Weight::normal));
    pwd->color(Palette::ColorId::text, Color(150, 150, 150));
    pwd->color(Palette::ColorId::bg, INPUT_BG);
    pwd->border(0);
    pwd->border_radius(4);
    input_row->add(pwd);

    // Figma inner shadow (top band) — readonly so taps reach the TextBox.
    input_row->add(make_shared<InputInnerShadow>(Rect(18, 14, 420, 12), 4.0f));

    // Estado para limpiar el placeholder solo la primera vez
    auto first_edit = make_shared<bool>(true);

    // Habilitar foco inicial para recibir teclado físico
    (void)pwd->focus();

    // Manejador: limpiar placeholder en click directo
    pwd->on_event([=](Event& event){
        if (*first_edit && event.id() == EventId::pointer_click) {
            *first_edit = false;
            pwd->text("");
            pwd->color(Palette::ColorId::text, Color(17, 24, 39));
        }
        return false;
    });

    // Capturar teclado físico global sin necesidad de foco previo
    main_frame->on_event([=](Event& event){
        if (event.id() == EventId::keyboard_down) {
            auto key = event.key();
            // Forzar foco al textbox
            (void)pwd->focus();
            if (*first_edit) {
                *first_edit = false;
                pwd->text("");
                pwd->color(Palette::ColorId::text, Color(17, 24, 39));
            }
            if (key.keycode == EKEY_BACKSPACE) {
                std::string curr = pwd->text();
                if (!curr.empty()) {
                    curr.pop_back();
                    pwd->text(curr);
                }
            } else if (key.keycode == EKEY_ENTER) {
                std::string curr = pwd->text();
                if (!curr.empty() && on_join) on_join(curr);
            } else if (key.unicode != 0) {
                // Caracter imprimible
                std::string curr = pwd->text();
                curr += static_cast<char>(key.unicode);
                pwd->text(curr);
            }
        }
        return false;
    });

    // Cancel + Join buttons rendered as PNGs from Figma so we get the exact
    // shadow + gradient look without re-painting in code. Figma button
    // bboxes (device): Cancel (459,85,141,54), Join (611,85,165,54).
    // PNG natural device sizes: Cancel 148x70, Join 172x70 (PNG_px * 0.926).
    // Inside input_row (origin at global (42, y_cursor)), the wraps land at:
    //   Cancel:  x = 459-42-(148-141)/2 = 413, y = 85-y_cursor-(70-54)/2
    //   Join:    x = 611-42-(172-165)/2 = 565
    // We use y=4 inside input_row so the buttons sit visually centred on
    // the password field (input_row is 70 px tall; the field is 44 at y=13).
    auto make_img_btn = [](const std::string& png_path, const Rect& r,
                           function<void()> on_click) {
        auto wrap = make_shared<Frame>(r);
        wrap->fill_flags({});
        try {
            auto probe = Image(("file:" + png_path).c_str());
            const float hs = static_cast<float>(r.width())  / probe.width();
            const float vs = static_cast<float>(r.height()) / probe.height();
            auto img = Image(("file:" + png_path).c_str(), hs, vs);
            auto lbl = make_shared<ImageLabel>(img);
            lbl->autoresize(false);
            lbl->border(0); lbl->padding(0); lbl->margin(0);
            lbl->fill_flags({});
            lbl->image_align(AlignFlag::center);
            lbl->box(Rect(0, 0, r.width(), r.height()));
            wrap->add(lbl);
        } catch (const std::exception& e) {
            printf("[PWD] image %s missing: %s\n", png_path.c_str(), e.what());
            fflush(stdout);
        }
        wrap->on_event([on_click](Event& e) {
            if (e.id() == EventId::pointer_click && on_click) on_click();
        });
        return wrap;
    };

    // Cancel at global x=459 - card x=10 -> input_row x=449
    // Join   at global x=611 - card x=10 -> input_row x=601
    auto btn_cancel = make_img_btn(
        ui::asset_path("pwd-btn-cancel"),
        Rect(449, 0, 148, 70),
        [=]() { if (on_cancel) on_cancel(); });
    input_row->add(btn_cancel);

    auto btn_join = make_img_btn(
        ui::asset_path("pwd-btn-join"),
        Rect(601, 0, 172, 70),
        [=]() { if (on_join) on_join(pwd->text()); });
    input_row->add(btn_join);

    // ── Forgot Password link (Figma 154:919) ───────────────────────────────
    // Centered below the input row. fontSize 12 Bold -> device 22 Bold.
    // figma top 397 -> device text top ≈161, centre ≈172: label top =
    // 172 - 15 (half label) - 10 (card y) = 147 = y_cursor + 91.
    auto forgot = make_shared<Label>("Forgot Password",
        Rect(0, y_cursor + 91, CARD_W, 30), AlignFlag::center);
    forgot->font(Font("Gothic A1", 22, Font::Weight::bold));
    forgot->color(Palette::ColorId::label_text, dt::kTextPrimary);
    card->add(forgot);

    // Keyboard frame — figma row 1 sits at global y=213 (figma 425):
    // card y 10 + keyboard_top 195 + sy 8 = 213. The frame runs to the card
    // bottom so row 4 (ends global y≈474) isn't clipped.
    int keyboard_top = 195;

    auto keyboard_frame = make_shared<Frame>(Rect(4, keyboard_top, CARD_W - 8, CARD_H - keyboard_top));
    keyboard_frame->color(Palette::ColorId::bg, dt::kWhite);
    card->add(keyboard_frame);

    auto shift_on = make_shared<bool>(false);
    int tw = CARD_W - 8;  // total keyboard width

    // Figma key metrics (figma px × 1.852): keys 31 figma tall ≈ 57 dev,
    // ~30-32 figma wide ≈ 58 dev, gaps 6 figma ≈ 11 dev, row pitch 68
    // (figma 68.5). Row 1 spans global x=28..776 and rows sit at global
    // y=213/281/349/417 (figma 213/281/350/419).
    const int kw = 58, kh = 57, gx = 11, gy = 11;
    const int rw       = 11 * kw + 10 * gx;       // row 1 outer extent (748)
    const int sx1      = (tw - rw) / 2 + 2;       // row 1 starts at global x=28
    const int return_w = kw + 48;                 // Return: 57 figma ≈ 106 dev
    const int shift_w  = kw;                      // figma shifts are letter-sized
    // Row 4 (figma 72:1578/1584/1581/1586): .?123 w126 / space w394 /
    // .?123 w100 / blank key w94 device px.
    const int nkw_l    = 126;
    const int nkw_r    = 100;
    const int blank_w  = 94;
    const int space_w  = rw - nkw_l - nkw_r - blank_w - 3 * gx; // 395
    const int content_h = 4 * kh + 3 * gy;
    const int sy = std::max(8, (CARD_H - keyboard_top - content_h) / 2);

    // Two sub-frames: QWERTY and numeric (toggle with ?123 / ABC)
    auto kb_alpha = make_shared<Frame>(Rect(0, 0, tw, CARD_H - keyboard_top));
    kb_alpha->fill_flags({Theme::FillFlag::blend});
    kb_alpha->color(Palette::ColorId::bg, Color(0, 0, 0, 0));
    keyboard_frame->add(kb_alpha);

    auto kb_num = make_shared<Frame>(Rect(0, 0, tw, CARD_H - keyboard_top));
    kb_num->fill_flags({Theme::FillFlag::blend});
    kb_num->color(Palette::ColorId::bg, Color(0, 0, 0, 0));
    kb_num->hide();
    keyboard_frame->add(kb_num);

    // Helper: type a character into pwd, clearing placeholder on first use
    auto type_ch = [pwd, first_edit](char c) {
        if (*first_edit) {
            *first_edit = false;
            pwd->text("");
            pwd->color(Palette::ColorId::text, Color(17, 24, 39));
        }
        pwd->text(pwd->text() + string(1, c));
    };
    auto do_bksp = [pwd]() {
        string t = pwd->text();
        if (!t.empty()) { t.pop_back(); pwd->text(t); }
    };
    auto do_enter = [pwd, on_join]() {
        string t = pwd->text();
        if (!t.empty() && on_join) on_join(t);
    };

    // Helper: create a styled keyboard key — custom-drawn KeyButton (figma:
    // gradient/flat fill, no stroke, r≈4, drop shadow 1px 1px 2px @0.2).
    auto mk = [](shared_ptr<Frame> parent, const string& label,
                  int x, int y, int w, int h,
                  function<void()> action, bool special = false) {
        // Letters: figma 14 Medium -> 26 dev. Multi-char ASCII special
        // labels (Return / .?123 / ABC): figma 10 -> 18 dev. The
        // single-glyph UTF-8 specials (← ⇧) keep the letter size.
        const bool small_label = special && label.size() > 1 &&
                                 static_cast<unsigned char>(label[0]) < 0x80;
        auto b = make_shared<KeyButton>(
            label, Rect(x, y, w, h), special,
            Font("Gothic A1", small_label ? 18 : 26, Font::Weight::normal),
            std::move(action));
        parent->add(b);
        return b;
    };

    auto switch_num = [kb_alpha, kb_num]() { kb_alpha->hide(); kb_num->show(); };
    auto switch_abc = [kb_alpha, kb_num]() { kb_alpha->show(); kb_num->hide(); };

    // Row-specific start x. Figma: rows 1, 3 and 4 all span x=28..776
    // device; row 2 is inset on the left but its Return key ends flush at
    // x=776, so row 2 is right-aligned with row 1.
    const int rw2 = 9 * kw + 9 * gx + return_w;
    const int sx2 = sx1 + rw - rw2;       // right-align (Return ends at 776)
    const int rw3 = 2 * shift_w + 9 * kw + 10 * gx;  // == rw
    const int sx3 = sx1 + (rw - rw3) / 2; // row 3 spans the row-1 extent
    const int sx4 = sx1;  // align row 4 to row 1's outer extent

    // ── QWERTY layout ──────────────────────────────────────────────
    {
        // Row 1: Q W E R T Y U I O P ←
        string r1 = "QWERTYUIOP";
        for (size_t i = 0; i < r1.size(); i++) {
            char c = r1[i];
            mk(kb_alpha, string(1, c),
               sx1 + (int)i * (kw + gx), sy, kw, kh,
               [type_ch, c, shift_on]() { type_ch(*shift_on ? c : (char)tolower(c)); });
        }
        mk(kb_alpha, "\xe2\x86\x90",
           sx1 + 10 * (kw + gx), sy, kw, kh, do_bksp, true);

        // Row 2: A S D F G H J K L Return — Return stretched (iOS)
        int y2 = sy + kh + gy;
        string r2 = "ASDFGHJKL";
        for (size_t i = 0; i < r2.size(); i++) {
            char c = r2[i];
            mk(kb_alpha, string(1, c),
               sx2 + (int)i * (kw + gx), y2, kw, kh,
               [type_ch, c, shift_on]() { type_ch(*shift_on ? c : (char)tolower(c)); });
        }
        mk(kb_alpha, "Return",
           sx2 + 9 * (kw + gx), y2, return_w, kh, do_enter, true);

        // Row 3: ⇧ Z X C V B N M , ? ⇧ — shifts wider than letters (iOS)
        int y3 = y2 + kh + gy;
        mk(kb_alpha, "\xe2\x87\xa7", sx3, y3, shift_w, kh,
           [shift_on]() { *shift_on = !(*shift_on); }, true);
        string r3 = "ZXCVBNM";
        for (size_t i = 0; i < r3.size(); i++) {
            char c = r3[i];
            mk(kb_alpha, string(1, c),
               sx3 + shift_w + gx + (int)i * (kw + gx), y3, kw, kh,
               [type_ch, c, shift_on]() { type_ch(*shift_on ? c : (char)tolower(c)); });
        }
        mk(kb_alpha, ",",
           sx3 + shift_w + gx + 7 * (kw + gx), y3, kw, kh,
           [type_ch]() { type_ch(','); });
        mk(kb_alpha, "?",
           sx3 + shift_w + gx + 8 * (kw + gx), y3, kw, kh,
           [type_ch]() { type_ch('?'); });
        mk(kb_alpha, "\xe2\x87\xa7",
           sx3 + shift_w + gx + 9 * (kw + gx), y3, shift_w, kh,
           [shift_on]() { *shift_on = !(*shift_on); }, true);

        // Row 4: ?123 [space] ?123 [blank] — figma 4-key layout, outer
        // edges aligned with row 1 (blank key 72:1586 has no label/action).
        int y4 = y3 + kh + gy;
        mk(kb_alpha, ".?123", sx4, y4, nkw_l, kh, switch_num, true);
        mk(kb_alpha, "",
           sx4 + nkw_l + gx, y4, space_w, kh,
           [type_ch]() { type_ch(' '); });
        mk(kb_alpha, ".?123",
           sx4 + nkw_l + gx + space_w + gx, y4, nkw_r, kh, switch_num, true);
        mk(kb_alpha, "",
           sx4 + nkw_l + gx + space_w + gx + nkw_r + gx, y4, blank_w, kh,
           nullptr, true);
    }

    // ── Numeric / Symbol layout ─────────────────────────────────────
    {
        // Row 1: 1 2 3 4 5 6 7 8 9 0 ←
        string n1 = "1234567890";
        for (size_t i = 0; i < n1.size(); i++) {
            char c = n1[i];
            mk(kb_num, string(1, c),
               sx1 + (int)i * (kw + gx), sy, kw, kh,
               [type_ch, c]() { type_ch(c); });
        }
        mk(kb_num, "\xe2\x86\x90",
           sx1 + 10 * (kw + gx), sy, kw, kh, do_bksp, true);

        // Row 2: @ # $ _ & - + ( ) Return
        int y2 = sy + kh + gy;
        string n2 = "@#$_&-+()";
        for (size_t i = 0; i < n2.size(); i++) {
            char c = n2[i];
            mk(kb_num, string(1, c),
               sx2 + (int)i * (kw + gx), y2, kw, kh,
               [type_ch, c]() { type_ch(c); });
        }
        mk(kb_num, "Return",
           sx2 + 9 * (kw + gx), y2, return_w, kh, do_enter, true);

        // Row 3: = * " ' : ; ! ~ / . (10 slots, centred under row 1)
        int y3 = y2 + kh + gy;
        const char* n3_labels[] = {"=","*","\"","'",":",";","!","~","/","."};
        const char  n3_chars[]  = {'=','*','"','\'',':',';','!','~','/','.'};
        const int r3w_num = 10 * kw + 9 * gx;
        const int sx3_num = sx1 + (rw - r3w_num) / 2;
        for (int i = 0; i < 10; i++) {
            char c = n3_chars[i];
            mk(kb_num, n3_labels[i],
               sx3_num + i * (kw + gx), y3, kw, kh,
               [type_ch, c]() { type_ch(c); });
        }

        // Row 4: ABC [space] ABC [blank] — mirrors alpha layout
        int y4 = y3 + kh + gy;
        mk(kb_num, "ABC", sx4, y4, nkw_l, kh, switch_abc, true);
        mk(kb_num, "",
           sx4 + nkw_l + gx, y4, space_w, kh,
           [type_ch]() { type_ch(' '); });
        mk(kb_num, "ABC",
           sx4 + nkw_l + gx + space_w + gx, y4, nkw_r, kh, switch_abc, true);
        mk(kb_num, "",
           sx4 + nkw_l + gx + space_w + gx + nkw_r + gx, y4, blank_w, kh,
           nullptr, true);
    }

    return main_frame;
}
