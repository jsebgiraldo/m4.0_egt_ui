#include <egt/ui>
#include <egt/virtualkeyboard.h>
#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <cctype>

using namespace egt;
using namespace std;

// Constantes de diseño y colores - EXPANDIDO PARA USAR MÁS ESPACIO
static constexpr int SCREEN_W       = 800;
static constexpr int SCREEN_H       = 480;
static constexpr int CARD_W         = 780;  // Expandido de 720 a 780 (más ancho)
static constexpr int CARD_H         = 460;  // Expandido de 420 a 460 (más alto)
static constexpr int PAD            = 15;   // Reducido de 25 a 15 (menos padding)
static constexpr int GAP_H          = 12;   // Reducido de 18 a 12

static const Color BTN_BLUE      = Color(30, 136, 229);
static const Color BTN_BLUE_DN   = Color(25, 118, 210);
static const Color CANCEL_BG     = Color(255, 255, 255);
static const Color CANCEL_FG     = Color(80, 80, 80);
static const Color CANCEL_BORDER = Color(215, 215, 215);
static const Color KEY_BG        = Color(255, 255, 255); // Fondo blanco puro para contraste máximo
static const Color KEY_BORDER    = Color(180, 180, 180); // Borde más oscuro
static const Color KEY_TEXT      = Color(0, 0, 0);       // Negro puro

shared_ptr<Widget> create_password_prompt_screen(
    const string& title_text,
    const string& message,
    const string& join_label,
    const string& cancel_label,
    function<void(const string&)> on_join,
    function<void()> on_cancel)
{
    // Frame principal que ocupará toda la pantalla
    auto main_frame = make_shared<Frame>(Rect(0, 0, SCREEN_W, SCREEN_H));
    main_frame->color(Palette::ColorId::bg, Color(0, 0, 0, 150));

    // Tarjeta central con posición absoluta
    auto card = make_shared<Frame>(
        Rect((SCREEN_W - CARD_W) / 2, (SCREEN_H - CARD_H) / 2, CARD_W, CARD_H));
    card->color(Palette::ColorId::bg, Palette::white);
    card->border_radius(12);
    main_frame->add(card);

    // Título centrado en la parte superior
    auto title = make_shared<Label>(
        title_text.empty() ? "Enter Password" : title_text);
    title->resize(Size(CARD_W - PAD*2, 35));
    title->move(Point(PAD, PAD + 5));  // Posición superior optimizada
    title->font(Font(18));
    title->align(AlignFlag::center);
    title->color(Palette::ColorId::label_text, Palette::black);
    card->add(title);

    // Campo de password más ancho, centrado horizontalmente
    auto pwd = make_shared<TextBox>("Password..");
    pwd->resize(Size(CARD_W - 180, 42));
    pwd->move(Point(40, 55));  // Posición ajustada según la imagen
    pwd->font(Font(16));
    pwd->color(Palette::ColorId::bg, KEY_BG);
    pwd->color(Palette::ColorId::border, KEY_BORDER);
    pwd->border(1);
    pwd->border_radius(6);
    card->add(pwd);

    // Botón Cancel - posición según imagen (izquierda)
    auto btn_cancel = make_shared<Button>(
        cancel_label.empty() ? "Cancel" : cancel_label);
    btn_cancel->resize(Size(75, 42));
    btn_cancel->move(Point(CARD_W - 165, 55));  // Alineado con el campo de password
    btn_cancel->font(Font(14));
    btn_cancel->color(Palette::ColorId::button_bg, CANCEL_BG);
    btn_cancel->color(Palette::ColorId::label_text, CANCEL_FG);
    btn_cancel->color(Palette::ColorId::border, CANCEL_BORDER);
    btn_cancel->border(1);
    btn_cancel->border_radius(8);
    btn_cancel->on_click([=](Event&){ if (on_cancel) on_cancel(); });
    card->add(btn_cancel);

    // Botón Join - al lado del Cancel según la imagen
    auto btn_join = make_shared<Button>(
        join_label.empty() ? "Join" : join_label);
    btn_join->resize(Size(75, 42));
    btn_join->move(Point(CARD_W - 85, 55));  // Justo al lado del Cancel
    btn_join->font(Font(14, Font::Weight::bold));
    btn_join->color(Palette::ColorId::button_bg, BTN_BLUE);
    btn_join->color(Palette::ColorId::label_text, Palette::white);
    btn_join->color(Palette::ColorId::border, BTN_BLUE_DN);
    btn_join->border(1);
    btn_join->border_radius(6);
    btn_join->on_click([=](Event&){ if (on_join) on_join(pwd->text()); });
    card->add(btn_join);

    // Crear teclado personalizado MOVIDO HACIA ABAJO para coincidir con la imagen
    auto keyboard_frame = make_shared<Frame>(Rect(PAD, 170, CARD_W - PAD*2, 270));
    keyboard_frame->color(Palette::ColorId::bg, Palette::white);
    card->add(keyboard_frame);

    // Función helper para crear teclas con Frame + Label para control total de colores
    auto create_key = [=](const string& text, int x, int y, int w, int h, 
                          std::function<void()> on_press) -> shared_ptr<Frame> {
        auto key_frame = make_shared<Frame>(Rect(x, y, w, h));
        
        // Configurar el frame como botón
        key_frame->color(Palette::ColorId::bg, KEY_BG);
        key_frame->color(Palette::ColorId::border, KEY_BORDER);
        key_frame->border(1);
        key_frame->border_radius(6);
        
        // Crear label con texto negro
        auto label = make_shared<Label>(text);
        label->resize(Size(w, h));
        label->move(Point(0, 0));
        label->font(Font(14));
        label->color(Palette::ColorId::label_text, KEY_TEXT);  // Negro puro
        label->align(AlignFlag::center);
        
        key_frame->add(label);
        
        // Manejar clicks
        if (on_press) {
            key_frame->on_event([on_press](Event& event) {
                if (event.id() == EventId::pointer_click) {
                    on_press();
                    return true;
                }
                return false;
            });
        }
        
        keyboard_frame->add(key_frame);
        return key_frame;
    };

    // AJUSTAR PARA EVITAR BORDES CORTADOS
    int total_width = CARD_W - PAD*2;  // Ancho total disponible (750px aprox)
    int total_height = 270;            // Alto del teclado ajustado
    
    // Dimensiones AJUSTADAS para que quepan con márgenes adecuados
    int key_w = 58;                    // Teclas ligeramente más pequeñas para que quepan
    int key_h = 55;                    // Teclas ajustadas al alto disponible
    int gap_x = 4;                     // Espacios reducidos para evitar desborde
    int gap_y = 6;                     // Espacios verticales mantenidos
    
    // Centrar horizontalmente CON MÁRGENES DE SEGURIDAD
    int total_keys_width = 11 * key_w + 10 * gap_x; // Para 11 teclas en fila 1
    int margin_x = 10;                 // Margen mínimo en cada lado
    int start_x = margin_x + (total_width - total_keys_width - 2*margin_x) / 2;
    int start_y = 15;                  // Margen superior en el teclado

    // Fila 1: Q W E R T Y U I O P ⌫ (11 teclas distribuidas)
    string row1_chars = "QWERTYUIOP";
    for (size_t i = 0; i < row1_chars.length(); i++) {
        char c = row1_chars[i];
        create_key(string(1, c), 
                   start_x + i * (key_w + gap_x), 
                   start_y, key_w, key_h, [=]() {
            string curr = pwd->text();
            curr += tolower(c);
            pwd->text(curr);
        });
    }
    
    // Backspace al final de fila 1
    create_key("⌫", 
               start_x + 10 * (key_w + gap_x), 
               start_y, key_w, key_h, [=]() {
        string curr = pwd->text();
        if (!curr.empty()) {
            curr.pop_back();
            pwd->text(curr);
        }
    });

    // Fila 2: A S D F G H J K L Return (centrada CON márgenes)
    string row2_chars = "ASDFGHJKL";
    int row2_y = start_y + key_h + gap_y;
    int row2_width = 9 * key_w + 8 * gap_x + (key_w + 25); // 9 teclas + Return ajustada
    int start_x_row2 = margin_x + (total_width - row2_width - 2*margin_x) / 2;
    
    for (size_t i = 0; i < row2_chars.length(); i++) {
        char c = row2_chars[i];
        create_key(string(1, c), 
                   start_x_row2 + i * (key_w + gap_x), 
                   row2_y, key_w, key_h, [=]() {
            string curr = pwd->text();
            curr += tolower(c);
            pwd->text(curr);
        });
    }
    
    // Return (tecla ajustada para no desbordar)
    create_key("Return", 
               start_x_row2 + 9 * (key_w + gap_x), 
               row2_y, key_w + 25, key_h, [=]() {  // Reducida para evitar desborde
        if (on_join) on_join(pwd->text());
    });

    // Fila 3: ⇧ Z X C V B N M , ? ⇧ (ajustada para márgenes)
    int row3_y = row2_y + key_h + gap_y;
    int shift_w = key_w + 12; // Teclas Shift ajustadas para evitar desborde
    int row3_content_width = 2 * shift_w + 9 * key_w + 10 * gap_x; // Todo el contenido
    int start_x_row3 = margin_x + (total_width - row3_content_width - 2*margin_x) / 2;
    
    // Shift izquierdo
    create_key("⇧", start_x_row3, row3_y, shift_w, key_h, [=]() {
        // TODO: Toggle mayúsculas
    });
    
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
            else curr += tolower(c);
            pwd->text(curr);
        });
    }
    
    // Shift derecho
    create_key("⇧", 
               start_x_row3 + shift_w + gap_x + 9 * (key_w + gap_x), 
               row3_y, shift_w, key_h, [=]() {
        // TODO: Toggle mayúsculas
    });

    // Fila 4: ?123 [barra espaciadora] ?123 (AJUSTADA con márgenes)
    int row4_y = row3_y + key_h + gap_y;
    int num_key_w = key_w + 18;    // Teclas ?123 ajustadas para evitar desborde
    int space_w = total_width - 2 * num_key_w - 4 * gap_x - 2*margin_x; // Barra espaciadora respeta márgenes
    int start_x_row4 = margin_x;   // Comenzar con margen de seguridad
    
    // ?123 izquierdo (ajustado)
    create_key("?123", start_x_row4, row4_y, num_key_w, key_h, [=]() {
        // TODO: Cambiar a teclado numérico
    });
    
    // Barra espaciadora (ancha pero con márgenes respetados)
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
    pwd->focus();

    return main_frame;
}
