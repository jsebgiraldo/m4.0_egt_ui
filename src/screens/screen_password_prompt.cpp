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

    // Título superior centrado y visible
    auto title = make_shared<Label>(
        title_text.empty() ? "Enter Password" : title_text);
    title->resize(Size(CARD_W - PAD*2, 40));  // Altura adecuada
    title->move(Point(PAD, 12));  // Posición superior visible
    title->font(Font(18, Font::Weight::bold));  // Fuente bold para visibilidad
    title->align(AlignFlag::center);
    title->color(Palette::ColorId::label_text, Color(17, 24, 39));  // Color oscuro definido
    card->add(title);


    // Layout horizontal: campo de contraseña + botones de acción
    auto input_row = make_shared<Frame>(Rect(32, 60, CARD_W - 64, 70));  // Posición ajustada para dar espacio al título
    input_row->color(Palette::ColorId::bg, Color(0, 0, 0, 0)); // Transparente
    card->add(input_row);

    // Campo de password ajustado para mejor alineación con botones
    auto pwd = make_shared<TextBox>("Password...");  
    pwd->resize(Size(CARD_W - 280, 44));  // Más estrecho para dar más espacio a botones
    pwd->move(Point(0, 13));  // Centrado verticalmente en la fila más alta
    pwd->font(Font(16));
    pwd->color(Palette::ColorId::bg, Color(255, 255, 255));
    pwd->color(Palette::ColorId::border, Color(209, 213, 219));  // #D1D5DB
    pwd->color(Palette::ColorId::text, Color(17, 24, 39));  // #111827
    pwd->border(1);
    pwd->border_radius(8);  // Radio más pequeño para layout compacto
    input_row->add(pwd);

    // Botón Cancel - movido más a la izquierda
    auto btn_cancel = make_shared<Button>(
        cancel_label.empty() ? "Cancel" : cancel_label);
    btn_cancel->resize(Size(75, 44));  // Más compacto
    btn_cancel->move(Point(CARD_W - 225, 13));  // Centrado en la fila más alta
    btn_cancel->font(Font(14, Font::Weight::normal));
    btn_cancel->color(Palette::ColorId::button_bg, Color(229, 231, 235));  // #E5E7EB fondo gris
    btn_cancel->color(Palette::ColorId::label_text, Color(55, 65, 81));    // #374151 texto
    btn_cancel->color(Palette::ColorId::border, Color(209, 213, 219));     // #D1D5DB borde
    btn_cancel->border(1);
    btn_cancel->border_radius(8);
    btn_cancel->on_click([=](Event&){ if (on_cancel) on_cancel(); });
    input_row->add(btn_cancel);

    // Botón Join - movido más a la izquierda junto con Cancel
    auto btn_join = make_shared<Button>(
        join_label.empty() ? "Join" : join_label);
    btn_join->resize(Size(75, 44));  // Mismo tamaño que Cancel
    btn_join->move(Point(CARD_W - 140, 13));  // Centrado en la fila más alta
    btn_join->font(Font(14, Font::Weight::bold));
    btn_join->color(Palette::ColorId::button_bg, Color(37, 99, 235));  // #2563EB 
    btn_join->color(Palette::ColorId::label_text, Color(255, 255, 255)); // Blanco
    btn_join->color(Palette::ColorId::border, Color(37, 99, 235));       // Mismo azul
    btn_join->border(0);  // Sin borde
    btn_join->border_radius(8);
    btn_join->on_click([=](Event&){ if (on_join) on_join(pwd->text()); });
    input_row->add(btn_join);

    // Teclado virtual con posición optimizada
    auto keyboard_frame = make_shared<Frame>(Rect(32, 150, CARD_W - 64, 350));  // Posición ajustada
    keyboard_frame->color(Palette::ColorId::bg, Palette::white);
    card->add(keyboard_frame);

    // Estado para mayúsculas/minúsculas
    auto shift_state = make_shared<bool>(false);

    // Función helper para crear teclas con estilo unificado
    auto create_key = [=](const string& text, int x, int y, int w, int h, 
                          std::function<void()> on_press) -> shared_ptr<Frame> {
        auto key_frame = make_shared<Frame>(Rect(x, y, w, h));
        
        // Estilo unificado: fondo #F3F4F6, borde #E5E7EB, radio 10-12px
        key_frame->color(Palette::ColorId::bg, Color(243, 244, 246));  // #F3F4F6
        key_frame->color(Palette::ColorId::border, Color(229, 231, 235));  // #E5E7EB
        key_frame->border(1);
        key_frame->border_radius(10);  // Radio 10px
        
        // Crear label con texto #111827, tamaño reducido
        auto label = make_shared<Label>(text);
        label->resize(Size(w, h));
        label->move(Point(0, 0));
        label->font(Font(13, Font::Weight::normal));  // Un punto menos que el actual
        label->color(Palette::ColorId::label_text, Color(17, 24, 39));  // #111827
        label->align(AlignFlag::center);
        
        key_frame->add(label);
        
        // Efecto hover y click con elevación sutil
        if (on_press) {
            key_frame->on_event([=](Event& event) mutable {
                if (event.id() == EventId::pointer_click) {
                    // Efecto pressed
                    key_frame->color(Palette::ColorId::bg, Color(229, 231, 235));  // Más oscuro
                    on_press();
                    // Restaurar
                    key_frame->color(Palette::ColorId::bg, Color(243, 244, 246));
                    return true;
                }
                return false;
            });
        }
        
        keyboard_frame->add(key_frame);
        return key_frame;
    };

    // Dimensiones optimizadas del teclado para ocupar máximo espacio horizontal
    int total_width = CARD_W - 64;     // Ancho alineado con el campo de password
    int total_height = 350;            // Alto máximo expandido del teclado
    
    // Dimensiones optimizadas: teclas ajustadas para ocupar todo el ancho
    int key_w = 54;                    // Teclas optimizadas para encajar mejor
    int key_h = 55;                    // Teclas más altas para mejor touch
    int gap_x = 5;                     // Gap horizontal reducido para mejor aprovechamiento
    int gap_y = 12;                    // Gap vertical aumentado para mejor separación
    
    // Centrar horizontalmente SIN márgenes innecesarios para maximizar espacio
    int total_keys_width = 11 * key_w + 10 * gap_x; // Para 11 teclas en fila 1
    int margin_x = 5;                  // Margen mínimo reducido
    int start_x = (total_width - total_keys_width) / 2; // Centrado perfecto
    int start_y = 15;                  // Margen superior en el teclado

    // Función para toggle de mayúsculas
    auto toggle_shift = [=]() {
        *shift_state = !(*shift_state);
        // TODO: Actualizar visual de las teclas shift
    };

    // Fila 1: Q W E R T Y U I O P ← (11 teclas distribuidas)
    string row1_chars = "QWERTYUIOP";
    for (size_t i = 0; i < row1_chars.length(); i++) {
        char c = row1_chars[i];
        create_key(string(1, c), 
                   start_x + i * (key_w + gap_x), 
                   start_y, key_w, key_h, [=]() {
            string curr = pwd->text();
            char char_to_add = *shift_state ? c : tolower(c);
            curr += char_to_add;
            pwd->text(curr);
        });
    }
    
    // Backspace con ícono más claro
    create_key("←", 
               start_x + 10 * (key_w + gap_x), 
               start_y, key_w, key_h, [=]() {
        string curr = pwd->text();
        if (!curr.empty()) {
            curr.pop_back();
            pwd->text(curr);
        }
    });

    // Fila 2: A S D F G H J K L Return (centrada y optimizada)
    string row2_chars = "ASDFGHJKL";
    int row2_y = start_y + key_h + gap_y;
    int row2_width = 9 * key_w + 8 * gap_x + (key_w + 25); // 9 teclas + Return optimizada
    int start_x_row2 = (total_width - row2_width) / 2; // Centrado perfecto
    
    for (size_t i = 0; i < row2_chars.length(); i++) {
        char c = row2_chars[i];
        create_key(string(1, c), 
                   start_x_row2 + i * (key_w + gap_x), 
                   row2_y, key_w, key_h, [=]() {
            string curr = pwd->text();
            char char_to_add = *shift_state ? c : tolower(c);
            curr += char_to_add;
            pwd->text(curr);
        });
    }
    
    // Return con lógica mejorada: Join si campo no está vacío
    create_key("Return", 
               start_x_row2 + 9 * (key_w + gap_x), 
               row2_y, key_w + 25, key_h, [=]() {  // Tecla Return optimizada
        string curr = pwd->text();
        if (!curr.empty() && on_join) {
            on_join(curr);  // Dispara Join si hay contenido
        } else {
            curr += "\n";   // Inserta Enter si está vacío
            pwd->text(curr);
        }
    });

    // Fila 3: ⇧ Z X C V B N M , ? ⇧ (optimizada para ocupar todo el ancho)
    int row3_y = row2_y + key_h + gap_y;
    int shift_w = key_w + 18; // Teclas Shift optimizadas
    int row3_content_width = 2 * shift_w + 9 * key_w + 10 * gap_x; // Todo el contenido
    int start_x_row3 = (total_width - row3_content_width) / 2; // Centrado perfecto
    
    // Shift izquierdo con funcionalidad
    create_key("⇧", start_x_row3, row3_y, shift_w, key_h, toggle_shift);
    
    // Teclas centrales Z X C V B N M , ?
    string row3_chars = "ZXCVBNM,?";
    for (size_t i = 0; i < row3_chars.length(); i++) {
        char c = row3_chars[i];
        create_key(string(1, c), 
                   start_x_row3 + shift_w + gap_x + i * (key_w + gap_x), 
                   row3_y, key_w, key_h, [=]() {
            string curr = pwd->text();
            if (c == ',') curr += ",";
            else if (c == '?') curr += "?";
            else {
                char char_to_add = *shift_state ? toupper(c) : tolower(c);
                curr += char_to_add;
            }
            pwd->text(curr);
        });
    }
    
    // Shift derecho con la misma funcionalidad
    create_key("⇧", 
               start_x_row3 + shift_w + gap_x + 9 * (key_w + gap_x), 
               row3_y, shift_w, key_h, toggle_shift);

    // Fila 4: ?123 [barra espaciadora] ?123 (optimizada para ocupar todo el ancho)
    int row4_y = row3_y + key_h + gap_y;
    int num_key_w = key_w + 20;    // Teclas ?123 optimizadas
    int space_w = total_width - 2 * num_key_w - 4 * gap_x; // Barra espaciadora maximizada
    int start_x_row4 = (total_width - (2 * num_key_w + space_w + 2 * gap_x)) / 2; // Centrado perfecto
    
    // ?123 izquierdo (optimizado)
    create_key("?123", start_x_row4, row4_y, num_key_w, key_h, [=]() {
        // TODO: Cambiar a teclado numérico
    });
    
    // Barra espaciadora maximizada (sin texto visible)
    create_key("", 
               start_x_row4 + num_key_w + gap_x, 
               row4_y, space_w, key_h, [=]() {
        string curr = pwd->text();
        curr += " ";
        pwd->text(curr);
    });
    
    // ?123 derecho
    create_key("?123", 
               start_x_row4 + num_key_w + gap_x + space_w + gap_x, 
               row4_y, num_key_w, key_h, [=]() {
        // TODO: Cambiar a teclado numérico
    });

    // Foco inicial
    (void)pwd->focus();

    return main_frame;
}
