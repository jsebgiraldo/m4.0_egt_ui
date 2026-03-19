#include <egt/ui>
#include <egt/virtualkeyboard.h>
#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <cctype>

using namespace egt;
using namespace std;

// Constantes de diseño y colores - TOKENS DE TEMA EGT
static constexpr int SCREEN_W       = 800;
static constexpr int SCREEN_H       = 480;
static constexpr int CARD_W         = 780;  
static constexpr int CARD_H         = 460;  
static constexpr int PAD            = 15;   
static constexpr int GAP_H          = 12;   

// Tokens de colores del tema
static const Color kColorPrimary     = Color(59, 130, 246);   // #3B82F6
static const Color kColorPrimaryDark = Color(37, 99, 235);    // #2563EB
static const Color kGray100          = Color(243, 244, 246);  // #F3F4F6
static const Color kGray300          = Color(209, 213, 219);  // #D1D5DB
static const Color kGray700          = Color(55, 65, 81);     // #374151
static const Color kGray900          = Color(17, 24, 39);     // #111827
static const Color kWhite            = Color(255, 255, 255);  // #FFFFFF

// Tokens de radio
static constexpr int kRadiusSm       = 8;
static constexpr int kRadiusLg       = 12;

// Colores específicos mantenidos para compatibilidad
static const Color BTN_BLUE      = kColorPrimary;
static const Color BTN_BLUE_DN   = kColorPrimaryDark;
static const Color CANCEL_BG     = Color(229, 231, 235);     // #E5E7EB
static const Color CANCEL_FG     = kGray700;
static const Color CANCEL_BORDER = kGray300;
static const Color KEY_BG        = kGray100;
static const Color KEY_BORDER    = Color(229, 231, 235);     // #E5E7EB
static const Color KEY_TEXT      = kGray900;
static const Color INPUT_BG      = kWhite;
static const Color INPUT_BORDER  = kGray300;

shared_ptr<Widget> create_password_prompt_screen(
    const string& title_text,
    const string& message,
    const string& join_label,
    const string& cancel_label,
    function<void(const string&)> on_join,
    function<void()> on_cancel)
{
    // Frame principal con fondo semitransparente mejorado
    auto main_frame = make_shared<Frame>(Rect(0, 0, SCREEN_W, SCREEN_H));
    main_frame->color(Palette::ColorId::bg, Color(0, 0, 0, 120));  // Menos opaco para look más moderno

    // Tarjeta central con estilo mejorado
    auto card = make_shared<Frame>(
        Rect((SCREEN_W - CARD_W) / 2, (SCREEN_H - CARD_H) / 2, CARD_W, CARD_H));
    card->color(Palette::ColorId::bg, Color(255, 255, 255));
    card->color(Palette::ColorId::border, Color(230, 230, 235));
    card->border(1);
    card->border_radius(16);  // Bordes más redondeados para look moderno
    main_frame->add(card);

    // ---- INICIO: Layout dinámico superior ----
    int y_cursor = 12; // margen superior mínimo

    // Título (usa title_text si viene, sino fallback)
    auto title = make_shared<Label>(title_text.empty() ? string("Enter Password") : title_text);
    title->resize(Size(CARD_W - PAD * 2, 34));
    title->move(Point(PAD, y_cursor));
    title->font(Font(22, Font::Weight::bold));
    title->color(Palette::ColorId::label_text, Color(17, 24, 39));
    card->add(title);
    y_cursor += 34; // avanzar debajo del título

    // Mensaje opcional
    if (!message.empty()) {
        y_cursor += 4; // pequeño gap
        auto subtitle = make_shared<Label>(message);
        subtitle->resize(Size(CARD_W - PAD * 2, 22));
        subtitle->move(Point(PAD, y_cursor));
        subtitle->font(Font(14));
        subtitle->color(Palette::ColorId::label_text, Color(90, 90, 90));
        card->add(subtitle);
        y_cursor += 22; // avanzar debajo del subtitle
    }

    y_cursor += 10; // espacio antes del input
    // ---- FIN: Layout dinámico superior ----

    // Layout horizontal: campo de contraseña + botones de acción
    auto input_row = make_shared<Frame>(Rect(32, y_cursor, CARD_W - 64, 70));
    input_row->color(Palette::ColorId::bg, Color(0, 0, 0, 0));
    card->add(input_row);

    // Campo de password
    auto pwd = make_shared<TextBox>("Password...");
    pwd->resize(Size(CARD_W - 280, 44));
    pwd->move(Point(0, 13));
    pwd->font(Font(16));
    // Placeholder gris claro inicial
    pwd->color(Palette::ColorId::text, Color(150, 150, 150));
    pwd->color(Palette::ColorId::bg, Color(255, 255, 255));
    pwd->color(Palette::ColorId::border, Color(209, 213, 219));
    pwd->border(1);
    pwd->border_radius(8);
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

    // Botón Cancel
    auto btn_cancel = make_shared<Button>(cancel_label.empty() ? "Cancel" : cancel_label);
    btn_cancel->resize(Size(75, 44));
    btn_cancel->move(Point(CARD_W - 225, 13));
    btn_cancel->font(Font(14, Font::Weight::normal));
    btn_cancel->color(Palette::ColorId::button_bg, Color(229, 231, 235));
    btn_cancel->color(Palette::ColorId::label_text, Color(55, 65, 81));
    btn_cancel->color(Palette::ColorId::border, Color(209, 213, 219));
    btn_cancel->border(1);
    btn_cancel->border_radius(8);
    btn_cancel->on_click([=](Event&){ if (on_cancel) on_cancel(); });
    input_row->add(btn_cancel);

    // Botón Join
    auto btn_join = make_shared<Button>(join_label.empty() ? "Join" : join_label);
    btn_join->resize(Size(75, 44));
    btn_join->move(Point(CARD_W - 140, 13));
    btn_join->font(Font(14, Font::Weight::bold));
    btn_join->color(Palette::ColorId::button_bg, Color(37, 99, 235));
    btn_join->color(Palette::ColorId::label_text, Color(255, 255, 255));
    btn_join->color(Palette::ColorId::border, Color(37, 99, 235));
    btn_join->border(0);
    btn_join->border_radius(8);
    btn_join->on_click([=](Event&){ if (on_join) on_join(pwd->text()); });
    input_row->add(btn_join);

    // Recalcular y base del teclado según nueva altura ocupada
    int keyboard_top = y_cursor + 70 + 10;

    auto keyboard_frame = make_shared<Frame>(Rect(32, keyboard_top, CARD_W - 64, CARD_H - keyboard_top - 10));
    keyboard_frame->color(Palette::ColorId::bg, Palette::white);
    card->add(keyboard_frame);

    auto shift_on = make_shared<bool>(false);
    int tw = CARD_W - 64;  // total keyboard width
    int kw = 54, kh = 55, gx = 5, gy = 12, sy = 15;

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

    // Helper: create a styled keyboard Button (not Frame+Label)
    auto mk = [](shared_ptr<Frame> parent, const string& label,
                  int x, int y, int w, int h,
                  function<void()> action) {
        auto b = make_shared<Button>(label, Rect(x, y, w, h));
        b->font(Font(13, Font::Weight::normal));
        b->color(Palette::ColorId::button_bg, Color(243, 244, 246));
        b->color(Palette::ColorId::button_text, Color(17, 24, 39));
        b->color(Palette::ColorId::border, Color(229, 231, 235));
        b->border(1);
        b->border_radius(10);
        if (action) b->on_click([action](Event&) { action(); });
        parent->add(b);
        return b;
    };

    auto switch_num = [kb_alpha, kb_num]() { kb_alpha->hide(); kb_num->show(); };
    auto switch_abc = [kb_alpha, kb_num]() { kb_alpha->show(); kb_num->hide(); };

    // ── QWERTY layout ──────────────────────────────────────────────
    {
        int rw = 11 * kw + 10 * gx;
        int sx = (tw - rw) / 2;

        // Row 1: Q W E R T Y U I O P ←
        string r1 = "QWERTYUIOP";
        for (size_t i = 0; i < r1.size(); i++) {
            char c = r1[i];
            mk(kb_alpha, string(1, c),
               sx + (int)i * (kw + gx), sy, kw, kh,
               [type_ch, c, shift_on]() { type_ch(*shift_on ? c : (char)tolower(c)); });
        }
        mk(kb_alpha, "\xe2\x86\x90",
           sx + 10 * (kw + gx), sy, kw, kh, do_bksp);

        // Row 2: A S D F G H J K L Return
        int y2 = sy + kh + gy;
        string r2 = "ASDFGHJKL";
        int r2w = 9 * kw + 8 * gx + kw + 25;
        int sx2 = (tw - r2w) / 2;
        for (size_t i = 0; i < r2.size(); i++) {
            char c = r2[i];
            mk(kb_alpha, string(1, c),
               sx2 + (int)i * (kw + gx), y2, kw, kh,
               [type_ch, c, shift_on]() { type_ch(*shift_on ? c : (char)tolower(c)); });
        }
        mk(kb_alpha, "Return",
           sx2 + 9 * (kw + gx), y2, kw + 25, kh, do_enter);

        // Row 3: ⇧ Z X C V B N M , ? ⇧
        int y3 = y2 + kh + gy;
        int shw = kw + 18;
        string r3 = "ZXCVBNM";
        int r3w = 2 * shw + 9 * kw + 10 * gx;
        int sx3 = (tw - r3w) / 2;
        mk(kb_alpha, "\xe2\x87\xa7", sx3, y3, shw, kh,
           [shift_on]() { *shift_on = !(*shift_on); });
        for (size_t i = 0; i < r3.size(); i++) {
            char c = r3[i];
            mk(kb_alpha, string(1, c),
               sx3 + shw + gx + (int)i * (kw + gx), y3, kw, kh,
               [type_ch, c, shift_on]() { type_ch(*shift_on ? c : (char)tolower(c)); });
        }
        mk(kb_alpha, ",",
           sx3 + shw + gx + 7 * (kw + gx), y3, kw, kh,
           [type_ch]() { type_ch(','); });
        mk(kb_alpha, "?",
           sx3 + shw + gx + 8 * (kw + gx), y3, kw, kh,
           [type_ch]() { type_ch('?'); });
        mk(kb_alpha, "\xe2\x87\xa7",
           sx3 + shw + gx + 9 * (kw + gx), y3, shw, kh,
           [shift_on]() { *shift_on = !(*shift_on); });

        // Row 4: ?123 [space] ?123
        int y4 = y3 + kh + gy;
        int nkw = kw + 20;
        int spw = tw - 2 * nkw - 4 * gx;
        int sx4 = (tw - (2 * nkw + spw + 2 * gx)) / 2;
        mk(kb_alpha, "?123", sx4, y4, nkw, kh, switch_num);
        mk(kb_alpha, "",
           sx4 + nkw + gx, y4, spw, kh,
           [type_ch]() { type_ch(' '); });
        mk(kb_alpha, "?123",
           sx4 + nkw + gx + spw + gx, y4, nkw, kh, switch_num);
    }

    // ── Numeric / Symbol layout ─────────────────────────────────────
    {
        int rw = 11 * kw + 10 * gx;
        int sx = (tw - rw) / 2;

        // Row 1: 1 2 3 4 5 6 7 8 9 0 ←
        string n1 = "1234567890";
        for (size_t i = 0; i < n1.size(); i++) {
            char c = n1[i];
            mk(kb_num, string(1, c),
               sx + (int)i * (kw + gx), sy, kw, kh,
               [type_ch, c]() { type_ch(c); });
        }
        mk(kb_num, "\xe2\x86\x90",
           sx + 10 * (kw + gx), sy, kw, kh, do_bksp);

        // Row 2: @ # $ _ & - + ( ) Return
        int y2 = sy + kh + gy;
        string n2 = "@#$_&-+()";
        int r2w = 9 * kw + 8 * gx + kw + 25;
        int sx2 = (tw - r2w) / 2;
        for (size_t i = 0; i < n2.size(); i++) {
            char c = n2[i];
            mk(kb_num, string(1, c),
               sx2 + (int)i * (kw + gx), y2, kw, kh,
               [type_ch, c]() { type_ch(c); });
        }
        mk(kb_num, "Return",
           sx2 + 9 * (kw + gx), y2, kw + 25, kh, do_enter);

        // Row 3: = * " ' : ; ! ~ / .
        int y3 = y2 + kh + gy;
        const char* n3_labels[] = {"=","*","\"","'",":",";","!","~","/","."};
        const char  n3_chars[]  = {'=','*','"','\'',':',';','!','~','/','.'};
        int r3w = 10 * kw + 9 * gx;
        int sx3 = (tw - r3w) / 2;
        for (int i = 0; i < 10; i++) {
            char c = n3_chars[i];
            mk(kb_num, n3_labels[i],
               sx3 + i * (kw + gx), y3, kw, kh,
               [type_ch, c]() { type_ch(c); });
        }

        // Row 4: ABC [space] ABC
        int y4 = y3 + kh + gy;
        int nkw = kw + 20;
        int spw = tw - 2 * nkw - 4 * gx;
        int sx4 = (tw - (2 * nkw + spw + 2 * gx)) / 2;
        mk(kb_num, "ABC", sx4, y4, nkw, kh, switch_abc);
        mk(kb_num, "",
           sx4 + nkw + gx, y4, spw, kh,
           [type_ch]() { type_ch(' '); });
        mk(kb_num, "ABC",
           sx4 + nkw + gx + spw + gx, y4, nkw, kh, switch_abc);
    }

    return main_frame;
}
