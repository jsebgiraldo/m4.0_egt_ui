#include <egt/ui>
#include "screen_password_prompt.h"
#include <string>
#include <vector>
#include <functional>

using namespace egt;
using namespace std;

/*
 Objetivo: Ajustar tamaños y colores para que los botones grises sean
 uniformes, más compactos y cercanos al mockup (Pasted Image 2).
 Se eliminan sombras y exageraciones; todos los keys con mismo tono gris claro,
 bordes suaves y espaciado consistente. Filas centradas horizontalmente.
 Columna derecha: botón Join arriba, botón Return alineado con filas centrales.
*/

// ----------------- Ajustes de Layout -----------------
static constexpr int CARD_W         = 780;
static constexpr int CARD_H         = 470;
static constexpr int PAD            = 16;

static constexpr int FIELD_H        = 48;
static constexpr int GAP_H          = 14;
static constexpr int GAP_W          = 14;

static constexpr int KEY_H          = 44;    // altura de tecla (más pequeña)
static constexpr int KEY_FONT       = 18;
static constexpr int KEY_RADIUS     = 6;
static constexpr int ROW_VSPACE     = 8;     // espacio vertical entre filas
static constexpr int KB_HSPACE      = 10;    // ESPACIADO HORIZONTAL ENTRE TECLAS (faltaba)

static constexpr int RIGHT_COL_W    = 120;   // ancho columna derecha (Join / Return)

// ----------------- Colores -----------------
static const Color KEY_BG        = Color(245,245,245);
static const Color KEY_BG_DN     = Color(225,225,225);
static const Color KEY_BORDER    = Color(200,200,200);
static const Color KEY_TEXT      = Color(45,45,45);

static const Color BTN_BLUE      = Color(0,150,224);
static const Color BTN_BLUE_DN   = Color(0,120,190);

static const Color CANCEL_BG     = Color(250,250,250);
static const Color CANCEL_FG     = Color(55,55,55);
static const Color CANCEL_BORDER = Color(205,205,205);

// ----------------- Tecla estándar -----------------
static shared_ptr<Button> make_key(const string& txt,
                                   const Rect& r,
                                   bool bold = true)
{
    auto b = make_shared<Button>(txt, r);
    b->font(Font(KEY_FONT, bold ? Font::Weight::bold : Font::Weight::normal));
    b->color(Palette::ColorId::button_bg, KEY_BG);
    b->color(Palette::ColorId::button_fg, KEY_TEXT);
    b->color(Palette::ColorId::label_text, KEY_TEXT);
    b->color(Palette::ColorId::border, KEY_BORDER);
    b->border(1);
    b->border_radius(KEY_RADIUS);

    b->on_event([b](Event& e){
        if (e.id() == EventId::raw_pointer_down)
        {
            b->color(Palette::ColorId::button_bg, KEY_BG_DN);
            b->damage();
        }
        else if (e.id() == EventId::raw_pointer_up)
        {
            b->color(Palette::ColorId::button_bg, KEY_BG);
            b->damage();
        }
    });
    return b;
}

// ----------------- Pantalla -----------------
shared_ptr<Widget> create_password_prompt_screen(
    const string& title_text,
    const string& message,
    const string& join_label,
    const string& cancel_label,
    function<void(const string& password)> on_join,
    function<void()> on_cancel)
{
    auto overlay = make_shared<Frame>(Rect(0,0,800,480));
    overlay->color(Palette::ColorId::bg, Palette::white);

    auto card = make_shared<Frame>(Rect((800 - CARD_W)/2,
                                        (480 - CARD_H)/2,
                                        CARD_W, CARD_H));
    card->color(Palette::ColorId::bg, Palette::white);
    card->border(1);
    card->color(Palette::ColorId::border, Color(190,190,190));
    card->border_radius(12);
    overlay->add(card);

    int y = PAD;

    // Título
    auto title = make_shared<Label>(
        title_text.empty() ? "Enter Password" : title_text,
        Rect(PAD, y, CARD_W - PAD*2, 28));
    title->align(AlignFlag::center_horizontal);
    title->font(Font(22, Font::Weight::bold));
    card->add(title);
    y += 28 + (message.empty()? GAP_H : 4);

    // Mensaje
    if (!message.empty())
    {
        auto info = make_shared<Label>(message,
            Rect(PAD, y, CARD_W - PAD*2, 20));
        info->align(AlignFlag::center_horizontal);
        info->font(Font(14));
        info->color(Palette::ColorId::label_text, Color(90,90,90));
        card->add(info);
        y += 20 + GAP_H;
    }

    // Row: campo + Cancel + Join (Join a la derecha aparte)
    const int join_w    = RIGHT_COL_W;
    const int cancel_w  = 90;
    const int row_w     = CARD_W - PAD*2;
    const int right_res = join_w + GAP_W;
    int field_w         = row_w - cancel_w - GAP_W - right_res;

    // Campo password (placeholder simple)
    auto pwd = make_shared<TextBox>("Password..",
                                    Rect(PAD, y, field_w, FIELD_H));
    pwd->font(Font(18));
    pwd->border_radius(8);
    pwd->border(1);
    pwd->color(Palette::ColorId::bg, Color(248,248,248));
    pwd->color(Palette::ColorId::border, Color(185,185,185));
    card->add(pwd);
    auto first_input = make_shared<bool>(true);

    // Cancel
    int x_cancel = PAD + field_w + GAP_W;
    auto btn_cancel = make_shared<Button>(
        cancel_label.empty()? "Cancel" : cancel_label,
        Rect(x_cancel, y, cancel_w, FIELD_H));
    btn_cancel->font(Font(16));
    btn_cancel->color(Palette::ColorId::button_bg, CANCEL_BG);
    btn_cancel->color(Palette::ColorId::button_fg, CANCEL_FG);
    btn_cancel->color(Palette::ColorId::label_text, CANCEL_FG);
    btn_cancel->color(Palette::ColorId::border, CANCEL_BORDER);
    btn_cancel->border(1);
    btn_cancel->border_radius(8);
    btn_cancel->on_click([=](Event&){ if (on_cancel) on_cancel(); });
    card->add(btn_cancel);

    // Join (columna derecha)
    int x_join = CARD_W - PAD - join_w;
    auto btn_join = make_shared<Button>(
        join_label.empty()? "Join" : join_label,
        Rect(x_join, y, join_w, FIELD_H));
    btn_join->font(Font(16, Font::Weight::bold));
    btn_join->color(Palette::ColorId::button_bg, BTN_BLUE);
    btn_join->color(Palette::ColorId::button_fg, Palette::white);
    btn_join->color(Palette::ColorId::label_text, Palette::white);
    btn_join->color(Palette::ColorId::border, BTN_BLUE_DN);
    btn_join->border(1);
    btn_join->border_radius(8);
    btn_join->on_click([=](Event&){ if (on_join) on_join(pwd->text()); });
    btn_join->on_event([=](Event& e){
        if (e.id() == EventId::raw_pointer_down){
            btn_join->color(Palette::ColorId::button_bg, BTN_BLUE_DN);
            btn_join->damage();
        } else if (e.id() == EventId::raw_pointer_up){
            btn_join->color(Palette::ColorId::button_bg, BTN_BLUE);
            btn_join->damage();
        }
    });
    card->add(btn_join);

    y += FIELD_H + GAP_H;

    // ---------------- Teclado ----------------
    // Área izquierda (teclas) y área derecha (Return)
    int key_area_w = (CARD_W - PAD*2) - RIGHT_COL_W - GAP_W;
    int key_area_x = PAD;
    int right_col_x = PAD + key_area_w + GAP_W;
    int key_base_y = y;

    // Return (alto = 2 filas + espacio)
    auto btn_return = make_shared<Button>("Return",
        Rect(right_col_x,
             key_base_y + KEY_H + ROW_VSPACE,
             RIGHT_COL_W,
             KEY_H*2 + ROW_VSPACE));
    btn_return->font(Font(16, Font::Weight::bold));
    btn_return->color(Palette::ColorId::button_bg, BTN_BLUE);
    btn_return->color(Palette::ColorId::button_fg, Palette::white);
    btn_return->color(Palette::ColorId::label_text, Palette::white);
    btn_return->color(Palette::ColorId::border, BTN_BLUE_DN);
    btn_return->border(1);
    btn_return->border_radius(8);
    btn_return->on_click([=](Event&){ if (on_join) on_join(pwd->text()); });
    btn_return->on_event([=](Event& e){
        if (e.id() == EventId::raw_pointer_down){
            btn_return->color(Palette::ColorId::button_bg, BTN_BLUE_DN);
            btn_return->damage();
        } else if (e.id() == EventId::raw_pointer_up){
            btn_return->color(Palette::ColorId::button_bg, BTN_BLUE);
            btn_return->damage();
        }
    });
    card->add(btn_return);

    // Estado SHIFT
    auto shift_on = make_shared<bool>(false);

    auto add_char = [pwd, first_input, shift_on](char c){
        if (*first_input)
        {
            pwd->text("");  // limpia placeholder
            *first_input = false;
        }
        string t = pwd->text();
        if (*shift_on && c >= 'a' && c <= 'z')
            c = static_cast<char>(c - 32);
        pwd->text(t + string(1,c));
        *shift_on = false;
    };
    auto backspace = [pwd, first_input](){
        string t = pwd->text();
        if (*first_input) return;
        if (!t.empty())
        {
            t.pop_back();
            pwd->text(t);
        }
    };

    // Helper: construir fila centrada
    auto build_centered_row = [&](int row_index, const vector<string>& keys)
    {
        int n = (int)keys.size();
        int total_space = KB_HSPACE * (n - 1);
        int key_w = (key_area_w - total_space) / n;
        // Recalcular centro exacto (puede sobrar 1-2 px)
        int used_w = key_w * n + total_space;
        int start_x = key_area_x + (key_area_w - used_w) / 2;
        int y_row = key_base_y + row_index * (KEY_H + ROW_VSPACE);

        for (const auto& ktxt : keys)
        {
            Rect kr(start_x, y_row, key_w, KEY_H);
            auto kbtn = make_key(ktxt, kr);
            if (ktxt == "←")
                kbtn->on_click([=](Event&){ backspace(); });
            else if (ktxt == "⇧")
                kbtn->on_click([=](Event&){
                    *shift_on = !*shift_on;
                    kbtn->color(Palette::ColorId::button_bg,
                                *shift_on ? KEY_BG_DN : KEY_BG);
                    kbtn->damage();
                });
            else if (ktxt == "Space")
                kbtn->on_click([=](Event&){ add_char(' '); });
            else
                kbtn->on_click([=](Event&){ add_char(ktxt[0]); });

            card->add(kbtn);
            start_x += key_w + KB_HSPACE;
        }
    };

    // Filas de letras (centradas)
    build_centered_row(0, {"Q","W","E","R","T","Y","U","I","O","P","←"});
    build_centered_row(1, {"A","S","D","F","G","H","J","K","L"});
    build_centered_row(2, {"⇧","Z","X","C","V","B","N","M","⇧"});

    // Fila 4: .?123 | space grande | .?123 (centrado manual)
    {
        int row_index = 3;
        int y_row = key_base_y + row_index * (KEY_H + ROW_VSPACE);

        // Unidades: 1 / 5 / 1  (space = 5 unidades)
        int units = 1 + 5 + 1;
        int total_space = KB_HSPACE * (units - 1);
        int unit_w = (key_area_w - total_space) / units;
        int used_w = unit_w * units + total_space;
        int start_x = key_area_x + (key_area_w - used_w) / 2;

        auto left = make_key(".?123", Rect(start_x, y_row, unit_w, KEY_H));
        // (toggle numérico se puede implementar después)
        card->add(left);
        start_x += unit_w + KB_HSPACE;

        int space_w = unit_w * 5 + KB_HSPACE * 4;
        auto space = make_key("", Rect(start_x, y_row, space_w, KEY_H));
        space->on_click([=](Event&){ add_char(' '); });
        card->add(space);
        start_x += space_w + KB_HSPACE;

        auto right = make_key(".?123", Rect(start_x, y_row, unit_w, KEY_H));
        card->add(right);
    }

    // Enter físico (si existiera)
    pwd->on_event([=](Event& e){
        if (e.id() == EventId::keyboard_down)
        {
            auto k = e.key();
            if (k.keycode == 10 || k.keycode == 13)
                if (on_join) on_join(pwd->text());
        }
    });

    (void)pwd->focus();
    overlay->damage();
    return overlay;
}
