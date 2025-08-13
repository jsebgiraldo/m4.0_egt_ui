#include <egt/ui>
#include "screen_start.h"
#include <chrono>
#include <cmath>

using namespace egt;
using namespace std;

//------------------------------------------------------------------------------
// Aro gris (track) dibujado con la MISMA geometría que el progreso
//------------------------------------------------------------------------------
class RingTrack : public Frame
{
public:
    explicit RingTrack(const Rect& r, int stroke = 8)
        : Frame(r), m_stroke(stroke)
    {
        color(Palette::ColorId::bg, Color(0, 0, 0, 0)); // fondo totalmente transparente
        border(0);
    }

protected:
    void draw(Painter& p, const Rect& r) override
    {
        const float w = static_cast<float>(r.width());
        const float h = static_cast<float>(r.height());
        const float cx = w / 2.0f;
        const float cy = h / 2.0f;
        const float stroke = static_cast<float>(m_stroke);
        const float radius = std::min(w, h) / 2.0f - stroke / 2.0f;

        p.line_width(stroke);
        // Si tu backend lo soporta, puedes suavizar extremos:
        // p.line_cap(LineCap::round);

        p.set(Color(220, 220, 220)); // gris del track
        ArcF full(Point(cx, cy), radius, 0.0f, 2.0f * M_PI);
        p.arc(full);
        p.stroke();
    }

private:
    int m_stroke;
};

//------------------------------------------------------------------------------
// Progreso circular (verde) con la MISMA geometría que RingTrack
//------------------------------------------------------------------------------
class ProgressCircle : public Frame
{
public:
    explicit ProgressCircle(const Rect& r, int stroke = 8)
        : Frame(r), m_progress(0), m_stroke(stroke)
    {
        color(Palette::ColorId::bg, Color(0, 0, 0, 0));
        border(0);
    }

    void progress(int value)
    {
        m_progress = std::clamp(value, 0, 100);
        damage();
    }

protected:
    void draw(Painter& p, const Rect& r) override
    {
        if (m_progress <= 0) return;

        const float w = static_cast<float>(r.width());
        const float h = static_cast<float>(r.height());
        const float cx = w / 2.0f;
        const float cy = h / 2.0f;
        const float stroke = static_cast<float>(m_stroke);
        const float radius = std::min(w, h) / 2.0f - stroke / 2.0f;

        p.line_width(stroke);
        // p.line_cap(LineCap::round); // opcional
        p.set(Color(76, 175, 80)); // verde

        const float ang = (m_progress / 100.0f) * 2.0f * M_PI;
        ArcF arc(Point(cx, cy), radius, -M_PI / 2.0f, ang);
        p.arc(arc);
        p.stroke();
    }

private:
    int m_progress;
    int m_stroke;
};

//------------------------------------------------------------------------------
// Pantalla de inicio con Wi-Fi (usa RingTrack + ProgressCircle)
//------------------------------------------------------------------------------
shared_ptr<Widget> create_start_screen_with_wifi(
    function<void()> on_next,
    function<void()> on_connection_complete,
    function<void()> on_connection_failed)
{
    const int width  = 800;
    const int height = 480;

    auto container = make_shared<Frame>(Rect(0, 0, width, height));
    container->color(Palette::ColorId::bg, Palette::white);

    // Geometría del círculo
    const int circle_size = 280;
    const int circle_x = (width  - circle_size) / 2;
    const int circle_y = (height - circle_size) / 2 - 20;
    const int track = 8; // grosor de ambos aros

    // Contenedor del círculo (sin borde) para mantener layout
    auto circle_background = make_shared<Frame>(Rect(circle_x, circle_y, circle_size, circle_size));
    circle_background->border_radius(circle_size / 2);
    circle_background->color(Palette::ColorId::bg, Color(245, 245, 245)); // relleno suave
    circle_background->border(0); // <— sin border() para evitar desalineos
    container->add(circle_background);

    // MISMO rect local para track + progreso (reducido en 'track' para centrar el trazo)
    const Rect ring_rect(track / 2, track / 2, circle_size - track, circle_size - track);

    // Aro gris (debajo)
    auto ring_track = make_shared<RingTrack>(ring_rect, track);
    circle_background->add(ring_track);

    // Progreso verde (encima)
    auto progress_circle = make_shared<ProgressCircle>(ring_rect, track);
    circle_background->add(progress_circle);

    // Logo centrado (coordenadas globales)
    try
    {
        const int logo_w = 120;
        const int logo_h = 90;
        const int logo_x = (width - logo_w) / 2;
        const int logo_y = circle_y + (circle_size / 2) - (logo_h / 2) - 15;

        auto logo = make_shared<ImageLabel>(Image("file:assets/image/Lice-logo.png"));
        logo->resize(Size(logo_w, logo_h));
        logo->move(Point(logo_x, logo_y));
        container->add(logo);
    }
    catch (...)
    {
        // Placeholder sencillo si no hay logo
        const int logo_w = 120, logo_h = 90;
        const int logo_x = (width - logo_w) / 2;
        const int logo_y = circle_y + (circle_size / 2) - (logo_h / 2) - 15;

        auto ph = make_shared<Frame>(Rect(logo_x, logo_y, logo_w, logo_h));
        ph->color(Palette::ColorId::bg, Palette::lightgray);
        ph->border(2);
        ph->color(Palette::ColorId::border, Palette::gray);

        auto lbl = make_shared<Label>("Logo\nNot Found", Rect(0, 0, logo_w, logo_h));
        lbl->align(AlignFlag::center);
        lbl->font(Font(12));
        lbl->color(Palette::ColorId::label_text, Palette::red);
        ph->add(lbl);
        container->add(ph);
    }

    // Texto de estado
    const int text_y = circle_y + (circle_size / 2) + 40;
    auto subtitle = make_shared<Label>("Connecting to Wifi", Rect(0, text_y, width, 25));
    subtitle->align(AlignFlag::center_horizontal);
    subtitle->font(Font(14));
    subtitle->color(Palette::ColorId::label_text, Color(120, 120, 120));
    container->add(subtitle);

    // (Opcional) Botón continuar si quieres enganchar flujo
    // const int btn_y = circle_y + circle_size + 20;
    // auto btn = make_shared<Button>("Start", Rect(width/2 - 80, btn_y, 160, 40));
    // btn->align(AlignFlag::center_horizontal);
    // btn->font(Font(16, Font::Weight::bold));
    // btn->color(Palette::ColorId::button_bg, Color(76, 175, 80));
    // btn->color(Palette::ColorId::button_fg, Palette::white);
    // btn->border_radius(20);
    // btn->hide();
    // btn->on_click([=](Event&){ if (on_next) on_next(); });
    // container->add(btn);

    // Estado global
    auto connection_complete = make_shared<bool>(false);

    // Animación del progreso
    auto progress_value = make_shared<int>(0);
    auto progress_timer = make_shared<PeriodicTimer>(chrono::milliseconds(100));
    progress_timer->on_timeout([=]() {
        if (!*connection_complete)
        {
            (*progress_value) = ((*progress_value) + 2) % 101;
            progress_circle->progress(*progress_value);
        }
    });

    // Callbacks de estado (puedes adaptarlos a tu lógica real)
    auto handle_done = [&](const char* msg, const Color& c) {
        if (!*connection_complete)
        {
            *connection_complete = true;
            progress_timer->cancel();
            subtitle->text(msg);
            subtitle->color(Palette::ColorId::label_text, c);
        }
    };

    // Verificación inicial simulada
    auto initial_check = make_shared<PeriodicTimer>(chrono::seconds(1));
    initial_check->on_timeout([=]() {
        initial_check->cancel();
        subtitle->text("Connecting to Wifi");
        progress_timer->start();

        // Simular éxito a los 3s (ajusta a tu flujo Wi-Fi real)
        auto success_timer = make_shared<PeriodicTimer>(chrono::seconds(3));
        success_timer->on_timeout([=]() {
            success_timer->cancel();
            handle_done("Wi-Fi Connected", Color(76, 175, 80));
            if (on_connection_complete) on_connection_complete();
            // btn->show(); // si usas el botón de continuar
        });
        success_timer->start();
    });
    initial_check->start();

    return container;
}
