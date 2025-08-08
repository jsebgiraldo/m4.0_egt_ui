#include <egt/ui>
#include "screen_training.h"
#include <chrono>
#include <iomanip>
#include <sstream>

using namespace egt;
using namespace std;

shared_ptr<Widget> create_training_screen(
    int duration_seconds,
    function<void()> on_complete,
    function<void()> on_pause,
    function<void()> on_cancel)
{
    const int width = 800;
    const int height = 480;
    
    auto container = make_shared<Frame>(Rect(0, 0, width, height));
    container->color(Palette::ColorId::bg, Palette::white);

    // Estado del temporizador
    auto remaining_time = make_shared<int>(duration_seconds);
    auto timer_running = make_shared<bool>(false);
    auto timer = make_shared<PeriodicTimer>(chrono::seconds(1));

    // Función para formatear tiempo mm:ss
    auto format_time = [](int seconds) -> string {
        int minutes = seconds / 60;
        int secs = seconds % 60;
        stringstream ss;
        ss << setfill('0') << setw(2) << minutes << ":" << setw(2) << secs;
        return ss.str();
    };

    // Título
    auto title = make_shared<Label>("Training", Rect(0, 30, width, 60));
    title->align(AlignFlag::center_horizontal);
    title->font(Font(32, Font::Weight::bold));
    title->color(Palette::ColorId::label_text, Palette::blue);
    container->add(title);

    // Intensidad
    auto intensity_label = make_shared<Label>("Intensity: 70 mA", Rect(0, 110, width, 40));
    intensity_label->align(AlignFlag::center_horizontal);
    intensity_label->font(Font(24, Font::Weight::bold));
    intensity_label->color(Palette::ColorId::label_text, Palette::black);
    container->add(intensity_label);

    // Display del temporizador
    auto timer_display = make_shared<Label>(format_time(*remaining_time), Rect(0, 170, width, 80));
    timer_display->align(AlignFlag::center_horizontal);
    timer_display->font(Font(48, Font::Weight::bold));
    timer_display->color(Palette::ColorId::label_text, Palette::green);
    container->add(timer_display);

    // Botón Iniciar/Reanudar
    auto btn_start = make_shared<Button>("Start", Rect(100, 300, 150, 60));
    btn_start->font(Font(20, Font::Weight::bold));
    btn_start->color(Palette::ColorId::button_bg, Palette::green);
    btn_start->color(Palette::ColorId::button_fg, Palette::white);
    container->add(btn_start);

    // Botón Pausar
    auto btn_pause = make_shared<Button>("Pause", Rect(325, 300, 150, 60));
    btn_pause->font(Font(20, Font::Weight::bold));
    btn_pause->color(Palette::ColorId::button_bg, Palette::orange);
    btn_pause->color(Palette::ColorId::button_fg, Palette::white);
    btn_pause->disable(); // Inicialmente deshabilitado
    container->add(btn_pause);

    // Botón Cancelar
    auto btn_cancel = make_shared<Button>("Cancel", Rect(550, 300, 150, 60));
    btn_cancel->font(Font(20, Font::Weight::bold));
    btn_cancel->color(Palette::ColorId::button_bg, Palette::red);
    btn_cancel->color(Palette::ColorId::button_fg, Palette::white);
    btn_cancel->on_click([=](Event&) { 
        timer->cancel();
        on_cancel(); 
    });
    container->add(btn_cancel);

    // Configurar temporizador
    timer->on_timeout([=]() {
        (*remaining_time)--;
        timer_display->text(format_time(*remaining_time));
        
        // Cambiar color según el tiempo restante
        if (*remaining_time <= 10) {
            timer_display->color(Palette::ColorId::label_text, Palette::red);
        } else if (*remaining_time <= 30) {
            timer_display->color(Palette::ColorId::label_text, Palette::orange);
        }
        
        // Verificar si el tiempo se agotó
        if (*remaining_time <= 0) {
            timer->cancel();
            *timer_running = false;
            btn_start->text("Completed");
            btn_start->disable();
            btn_pause->disable();
            timer_display->color(Palette::ColorId::label_text, Palette::blue);
            on_complete();
        }
    });

    // Evento del botón Iniciar/Reanudar
    btn_start->on_click([=](Event&) {
        if (!*timer_running && *remaining_time > 0) {
            timer->start();
            *timer_running = true;
            btn_start->text("Running...");
            btn_start->disable();
            btn_pause->enable();
            timer_display->color(Palette::ColorId::label_text, Palette::green);
        }
    });

    // Evento del botón Pausar
    btn_pause->on_click([=](Event&) {
        if (*timer_running) {
            timer->cancel();
            *timer_running = false;
            btn_start->text("Resume");
            btn_start->enable();
            btn_pause->disable();
            timer_display->color(Palette::ColorId::label_text, Palette::orange);
            on_pause();
        }
    });

    // Información adicional
    auto info_label = make_shared<Label>("Keep electrodes in position", Rect(0, 390, width, 30));
    info_label->align(AlignFlag::center_horizontal);
    info_label->font(Font(16));
    info_label->color(Palette::ColorId::label_text, Palette::gray);
    container->add(info_label);

    return container;
}