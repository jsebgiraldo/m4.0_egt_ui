#include <egt/ui>
#include <egt/virtualkeyboard.h>
#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <cctype>

#include "../ui/design_tokens.h"

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

    // Mensaje opcional
    if (!message.empty()) {
        y_cursor += 4; // pequeño gap
        auto subtitle = make_shared<Label>(message);
        subtitle->resize(Size(CARD_W - PAD * 2, 22));
        subtitle->move(Point(PAD, y_cursor));
        subtitle->font(Font(14));
        subtitle->color(Palette::ColorId::label_text, dt::kTextPrimary);
        card->add(subtitle);
        y_cursor += 22; // avanzar debajo del subtitle
    }

    y_cursor += 10; // espacio antes del input
    // ---- FIN: Layout dinámico superior ----

    // Layout horizontal: campo de contraseña + botones de acción
    auto input_row = make_shared<Frame>(Rect(32, y_cursor, CARD_W - 64, 70));
    input_row->color(Palette::ColorId::bg, Color(0, 0, 0, 0));
    card->add(input_row);

    // Password field — Figma: gray fill, r=2, inner shadow
    auto pwd = make_shared<TextBox>("Password...");
    pwd->resize(Size(CARD_W - 280, 44));
    pwd->move(Point(0, 13));
    pwd->font(Font(16));
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

    // Cancel button — Figma: white fill, r=4, shadow, gray text
    auto btn_cancel = make_shared<Button>(cancel_label.empty() ? "Cancel" : cancel_label);
    btn_cancel->resize(Size(85, 44));
    btn_cancel->move(Point(CARD_W - 240, 13));
    btn_cancel->font(Font(14, Font::Weight::bold));
    btn_cancel->color(Palette::ColorId::button_bg, CANCEL_BG);
    btn_cancel->color(Palette::ColorId::button_text, CANCEL_FG);
    btn_cancel->color(Palette::ColorId::border, Color(220, 220, 220));
    btn_cancel->border(1);
    btn_cancel->border_radius(4);
    btn_cancel->on_click([=](Event&){ if (on_cancel) on_cancel(); });
    input_row->add(btn_cancel);

    // Join button — Figma: cyan→blue gradient, r=4, shadow, white text
    auto btn_join = make_shared<Button>(join_label.empty() ? "Join" : join_label);
    btn_join->resize(Size(85, 44));
    btn_join->move(Point(CARD_W - 145, 13));
    btn_join->font(Font(14, Font::Weight::bold));
    btn_join->color(Palette::ColorId::button_bg, JOIN_BG);
    btn_join->color(Palette::ColorId::button_text, dt::kWhite);
    btn_join->color(Palette::ColorId::border, JOIN_BG);
    btn_join->border(0);
    btn_join->border_radius(4);
    btn_join->on_click([=](Event&){ if (on_join) on_join(pwd->text()); });
    input_row->add(btn_join);

    // Recalcular y base del teclado según nueva altura ocupada
    int keyboard_top = y_cursor + 70 + 10;

    auto keyboard_frame = make_shared<Frame>(Rect(32, keyboard_top, CARD_W - 64, CARD_H - keyboard_top - 10));
    keyboard_frame->color(Palette::ColorId::bg, dt::kWhite);
    card->add(keyboard_frame);

    auto shift_on = make_shared<bool>(false);
    int tw = CARD_W - 64;  // total keyboard width

    // Key geometry. rw = reference row width (row 1, 11 keys + 10 gaps) is
    // the canvas the other rows must align to so every row starts/ends at
    // the same x. sy is computed so the 4-row block sits vertically centred
    // inside the keyboard_frame — no dead space at the bottom of the card.
    const int kw = 54, kh = 52, gx = 6, gy = 8;
    const int rw = 11 * kw + 10 * gx;
    const int kb_left = (tw - rw) / 2;
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
        b->font(Font(14, Font::Weight::normal));
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

    // Wide-key sizes — all rows align to row 1's left/right edges (kb_left
    // and kb_left+rw). The "stretch" keys (Return, shifts, ?123, space)
    // absorb the difference so the row edges match cleanly.
    const int return_w = rw - 9 * kw - 9 * gx;   // 9 letters + 9 gaps in row 2
    const int shift_w  = kw;                     // row 3 has 11 slots — keep uniform
    const int nkw      = kw + 24;                // ?123 / ABC corner keys
    const int space_w  = rw - 2 * nkw - 2 * gx;  // space bar absorbs the rest

    // ── QWERTY layout ──────────────────────────────────────────────
    {
        // Row 1: Q W E R T Y U I O P ←
        string r1 = "QWERTYUIOP";
        for (size_t i = 0; i < r1.size(); i++) {
            char c = r1[i];
            mk(kb_alpha, string(1, c),
               kb_left + (int)i * (kw + gx), sy, kw, kh,
               [type_ch, c, shift_on]() { type_ch(*shift_on ? c : (char)tolower(c)); });
        }
        mk(kb_alpha, "\xe2\x86\x90",
           kb_left + 10 * (kw + gx), sy, kw, kh, do_bksp, true);

        // Row 2: A S D F G H J K L Return — Return stretched to align right edge
        int y2 = sy + kh + gy;
        string r2 = "ASDFGHJKL";
        for (size_t i = 0; i < r2.size(); i++) {
            char c = r2[i];
            mk(kb_alpha, string(1, c),
               kb_left + (int)i * (kw + gx), y2, kw, kh,
               [type_ch, c, shift_on]() { type_ch(*shift_on ? c : (char)tolower(c)); });
        }
        mk(kb_alpha, "Return",
           kb_left + 9 * (kw + gx), y2, return_w, kh, do_enter, true);

        // Row 3: ⇧ Z X C V B N M , ? ⇧ — 11 slots, all kw wide
        int y3 = y2 + kh + gy;
        mk(kb_alpha, "\xe2\x87\xa7", kb_left, y3, shift_w, kh,
           [shift_on]() { *shift_on = !(*shift_on); }, true);
        string r3 = "ZXCVBNM";
        for (size_t i = 0; i < r3.size(); i++) {
            char c = r3[i];
            mk(kb_alpha, string(1, c),
               kb_left + (1 + (int)i) * (kw + gx), y3, kw, kh,
               [type_ch, c, shift_on]() { type_ch(*shift_on ? c : (char)tolower(c)); });
        }
        mk(kb_alpha, ",",
           kb_left + 8 * (kw + gx), y3, kw, kh,
           [type_ch]() { type_ch(','); });
        mk(kb_alpha, "?",
           kb_left + 9 * (kw + gx), y3, kw, kh,
           [type_ch]() { type_ch('?'); });
        mk(kb_alpha, "\xe2\x87\xa7",
           kb_left + 10 * (kw + gx), y3, shift_w, kh,
           [shift_on]() { *shift_on = !(*shift_on); }, true);

        // Row 4: ?123 [space] ?123 — space absorbs the rest of rw
        int y4 = y3 + kh + gy;
        mk(kb_alpha, "?123", kb_left, y4, nkw, kh, switch_num, true);
        mk(kb_alpha, "",
           kb_left + nkw + gx, y4, space_w, kh,
           [type_ch]() { type_ch(' '); });
        mk(kb_alpha, "?123",
           kb_left + nkw + gx + space_w + gx, y4, nkw, kh, switch_num, true);
    }

    // ── Numeric / Symbol layout ─────────────────────────────────────
    {
        // Row 1: 1 2 3 4 5 6 7 8 9 0 ←
        string n1 = "1234567890";
        for (size_t i = 0; i < n1.size(); i++) {
            char c = n1[i];
            mk(kb_num, string(1, c),
               kb_left + (int)i * (kw + gx), sy, kw, kh,
               [type_ch, c]() { type_ch(c); });
        }
        mk(kb_num, "\xe2\x86\x90",
           kb_left + 10 * (kw + gx), sy, kw, kh, do_bksp, true);

        // Row 2: @ # $ _ & - + ( ) Return
        int y2 = sy + kh + gy;
        string n2 = "@#$_&-+()";
        for (size_t i = 0; i < n2.size(); i++) {
            char c = n2[i];
            mk(kb_num, string(1, c),
               kb_left + (int)i * (kw + gx), y2, kw, kh,
               [type_ch, c]() { type_ch(c); });
        }
        mk(kb_num, "Return",
           kb_left + 9 * (kw + gx), y2, return_w, kh, do_enter, true);

        // Row 3: = * " ' : ; ! ~ / . (10 slots, centred between row 1 edges)
        int y3 = y2 + kh + gy;
        const char* n3_labels[] = {"=","*","\"","'",":",";","!","~","/","."};
        const char  n3_chars[]  = {'=','*','"','\'',':',';','!','~','/','.'};
        const int r3w = 10 * kw + 9 * gx;
        const int sx3 = kb_left + (rw - r3w) / 2;
        for (int i = 0; i < 10; i++) {
            char c = n3_chars[i];
            mk(kb_num, n3_labels[i],
               sx3 + i * (kw + gx), y3, kw, kh,
               [type_ch, c]() { type_ch(c); });
        }

        // Row 4: ABC [space] ABC — mirrors alpha layout
        int y4 = y3 + kh + gy;
        mk(kb_num, "ABC", kb_left, y4, nkw, kh, switch_abc, true);
        mk(kb_num, "",
           kb_left + nkw + gx, y4, space_w, kh,
           [type_ch]() { type_ch(' '); });
        mk(kb_num, "ABC",
           kb_left + nkw + gx + space_w + gx, y4, nkw, kh, switch_abc, true);
    }

    return main_frame;
}
