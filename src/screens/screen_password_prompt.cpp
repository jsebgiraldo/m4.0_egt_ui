#include <egt/ui>
#include <egt/virtualkeyboard.h>
#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <cctype>

#include "../ui/design_tokens.h"
#include "../ui/components.h"

using namespace egt;
using namespace std;

// Layout constants
static constexpr int SCREEN_W       = 800;
static constexpr int SCREEN_H       = 480;
static constexpr int CARD_W         = 780;
static constexpr int CARD_H         = 460;
static constexpr int PAD            = 15;

// Figma-matched colors
static const Color KEY_BG        = Color(244, 244, 244);     // gradient top approximation
static const Color KEY_SPECIAL   = Color(228, 229, 232);     // special key bg (backspace, Return, shift, .?123)
static const Color KEY_TEXT      = dt::kTextPrimary;          // rgb(100,101,105)
static const Color INPUT_BG      = Color(217, 217, 217, 128);// Figma: gray at 50% opacity
static const Color JOIN_BG       = dt::kAccentCyan;           // rgb(48,163,196) Figma gradient top
static const Color CANCEL_BG     = dt::kWhite;               // Figma: white fill
static const Color CANCEL_FG     = dt::kTextPrimary;         // rgb(100,101,105)

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

    // Card — minimal styling matching Figma Rectangle 9
    auto card = make_shared<Frame>(
        Rect((SCREEN_W - CARD_W) / 2, (SCREEN_H - CARD_H) / 2, CARD_W, CARD_H));
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
    auto input_row = make_shared<Frame>(Rect(0, y_cursor, CARD_W, 70));
    input_row->color(Palette::ColorId::bg, Color(0, 0, 0, 0));
    card->add(input_row);

    // Password field - Figma: gray fill, r=2, inner shadow.
    // Dimensions sampled from the Figma render at scale=2: the rectangle is
    // 227 figma px wide x 30.5 figma px tall -> 420 x 56 device px. Centred
    // vertically in the 70 px input_row (y = (70-56)/2 = 7).
    auto pwd = make_shared<TextBox>("Password..");
    pwd->resize(Size(420, 56));
    pwd->move(Point(10, 7));
    pwd->font(Font("Gothic A1", 22, Font::Weight::normal));
    pwd->color(Palette::ColorId::text, Color(150, 150, 150));
    pwd->color(Palette::ColorId::bg, INPUT_BG);
    pwd->color(Palette::ColorId::border, Color(200, 200, 200));
    pwd->border(1);
    pwd->border_radius(4);
    input_row->add(pwd);

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
    auto forgot = make_shared<Label>("Forgot Password",
        Rect(0, y_cursor + 75, CARD_W, 30), AlignFlag::center);
    forgot->font(Font("Gothic A1", 22, Font::Weight::bold));
    forgot->color(Palette::ColorId::label_text, dt::kTextPrimary);
    card->add(forgot);

    // Recalcular y base del teclado: input row (70) + Forgot Password (30) + gap
    int keyboard_top = y_cursor + 70 + 30 + 10;

    auto keyboard_frame = make_shared<Frame>(Rect(4, keyboard_top, CARD_W - 8, CARD_H - keyboard_top - 10));
    keyboard_frame->color(Palette::ColorId::bg, dt::kWhite);
    card->add(keyboard_frame);

    auto shift_on = make_shared<bool>(false);
    int tw = CARD_W - 8;  // total keyboard width (near-edge so 64 px keys fit)

    // iOS-style key geometry — stretch keys (Return, shift, ?123, space) are
    // visibly wider than letters. Row 1 is the alignment reference for rows
    // 2 and 4; row 3 ends up a touch wider than row 1 because its 11 slots
    // include 2 stretched shifts (same asymmetry iOS has). sy is computed so
    // the 4-row block sits vertically centred — no dead space at the bottom.
    // Key dimensions sampled from the Figma render: each key is roughly
    // 35 figma px square -> 64 device px. The keyboard frame uses CARD_W-8
    // so row 1 (11*64 + 60 = 764) fits inside tw = 772 with 4 px of slack
    // on each side.
    const int kw = 64, kh = 64, gx = 6, gy = 9;
    const int rw       = 11 * kw + 10 * gx;       // row 1 outer extent
    const int sx1      = (tw - rw) / 2;           // row 1 left margin
    const int return_w = kw + 50;                 // row 2 wide key (fits "Return" at 26 pt)
    const int shift_w  = kw + 18;                 // row 3 wide keys
    const int nkw      = kw + 22;                 // ?123 / ABC wide keys (row 4)
    const int space_w  = rw - 2 * nkw - 2 * gx;   // space fills row 4 to row 1 edges
    const int content_h = 4 * kh + 3 * gy;
    const int sy = std::max(8, (CARD_H - keyboard_top - 10 - content_h) / 2);

    // Two sub-frames: QWERTY and numeric (toggle with ?123 / ABC)
    auto kb_alpha = make_shared<Frame>(Rect(0, 0, tw, CARD_H - keyboard_top - 10));
    kb_alpha->fill_flags({Theme::FillFlag::blend});
    kb_alpha->color(Palette::ColorId::bg, Color(0, 0, 0, 0));
    keyboard_frame->add(kb_alpha);

    auto kb_num = make_shared<Frame>(Rect(0, 0, tw, CARD_H - keyboard_top - 10));
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

    // Helper: create a styled keyboard key — Figma: gradient fill, r=2, DROP_SHADOW
    auto mk = [](shared_ptr<Frame> parent, const string& label,
                  int x, int y, int w, int h,
                  function<void()> action, bool special = false) {
        auto b = make_shared<Button>(label, Rect(x, y, w, h));
        b->autoresize(false);  // bigger font would otherwise grow the button
        // Figma keyboard letters are fontSize 14 Medium -> device 26 pt.
        b->font(Font("Gothic A1", 26, Font::Weight::normal));
        b->color(Palette::ColorId::button_bg, special ? KEY_SPECIAL : KEY_BG);
        b->color(Palette::ColorId::button_text, KEY_TEXT);
        b->color(Palette::ColorId::border, Color(210, 210, 210));
        b->color(Palette::ColorId::button_bg, dt::kGreenLight, Palette::GroupId::active);
        b->color(Palette::ColorId::button_bg, dt::kGreenLight, Palette::GroupId::checked);
        b->border(1);
        b->border_radius(4);
        b->border_flags({Theme::BorderFlag::drop_shadow});
        if (action) b->on_click([action](Event&) { action(); });
        parent->add(b);
        return b;
    };

    auto switch_num = [kb_alpha, kb_num]() { kb_alpha->hide(); kb_num->show(); };
    auto switch_abc = [kb_alpha, kb_num]() { kb_alpha->show(); kb_num->hide(); };

    // Row-specific start x (each row centred within tw). Row 2 is narrower
    // than row 1 → looks inset (iOS look); row 3 is a touch wider thanks to
    // the stretched shifts (also iOS-like). Row 4 deliberately re-uses sx1
    // so its outer edges align with row 1 — that was the bug we fixed.
    const int rw2 = 9 * kw + 9 * gx + return_w;
    const int sx2 = (tw - rw2) / 2;
    const int rw3 = 2 * shift_w + 9 * kw + 10 * gx;
    const int sx3 = (tw - rw3) / 2;
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

        // Row 4: ?123 [space] ?123 — outer edges aligned with row 1
        int y4 = y3 + kh + gy;
        mk(kb_alpha, ".?123", sx4, y4, nkw, kh, switch_num, true);
        mk(kb_alpha, "",
           sx4 + nkw + gx, y4, space_w, kh,
           [type_ch]() { type_ch(' '); });
        mk(kb_alpha, ".?123",
           sx4 + nkw + gx + space_w + gx, y4, nkw, kh, switch_num, true);
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

        // Row 4: ABC [space] ABC — mirrors alpha layout
        int y4 = y3 + kh + gy;
        mk(kb_num, "ABC", sx4, y4, nkw, kh, switch_abc, true);
        mk(kb_num, "",
           sx4 + nkw + gx, y4, space_w, kh,
           [type_ch]() { type_ch(' '); });
        mk(kb_num, "ABC",
           sx4 + nkw + gx + space_w + gx, y4, nkw, kh, switch_abc, true);
    }

    return main_frame;
}
