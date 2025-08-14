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
        : Frame(r), m_progress(0.f), m_stroke(stroke)
    {
        color(Palette::ColorId::bg, Color(0, 0, 0, 0));
        border(0);
    }

    void progress(float value)
    {
        m_progress = std::clamp(value, 0.f, 100.f);
        damage();
    }

protected:
    void draw(Painter& p, const Rect& r) override
    {
        if (m_progress <= 0.f) return;

        const float w = static_cast<float>(r.width());
        const float h = static_cast<float>(r.height());
        const float cx = w / 2.0f;
        const float cy = h / 2.0f;
        const float stroke = static_cast<float>(m_stroke);
        const float radius = std::min(w, h) / 2.0f - stroke / 2.0f;

        p.line_width(stroke);
        p.line_cap(Painter::LineCap::round);
        p.set(Color(76, 175, 80)); // verde

        const float ang = (m_progress / 100.0f) * 2.0f * M_PI;
        ArcF arc(Point(cx, cy), radius, -M_PI / 2.0f, ang);
        p.arc(arc);
        p.stroke();
    }

private:
    float m_progress;
    int   m_stroke;
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

    //----- Ajustes de tamaño --------------------------------------------------
    const int circle_size = 280;      // diámetro visual original
    const int stroke      = 8;        // grosor de ambos aros
    const int padding     = 2;        // margen extra para que no se corte arriba/izquierda
    const int ring_diam   = circle_size + padding * 2;   // nuevo tamaño del frame que contiene el dibujo
    const int ring_x = (width  - ring_diam) / 2;
    const int ring_y = (height - ring_diam) / 2 - 20;

    // Contenedor (un poco más grande) para evitar clipping en top/left
    auto circle_background = make_shared<Frame>(Rect(ring_x, ring_y, ring_diam, ring_diam));
    circle_background->border_radius(ring_diam / 2);
    circle_background->color(Palette::ColorId::bg, Color(245, 245, 245));
    circle_background->border(0);
    container->add(circle_background);

    // Rect completo (ya NO lo reducimos) — el radio se ajusta internamente
    const Rect ring_rect(0, 0, ring_diam, ring_diam);

    // Aro gris
    auto ring_track = make_shared<RingTrack>(ring_rect, stroke);
    circle_background->add(ring_track);

    // Progreso verde
    auto progress_circle = make_shared<ProgressCircle>(ring_rect, stroke);
    circle_background->add(progress_circle);

    // Centro real (con padding) para posicionar logo y texto correctamente
    const int center_y = ring_y + ring_diam / 2;

    // Logo (sin cambios de tamaño)
    try
    {
        const int logo_w = 120;
        const int logo_h = 90;
        const int logo_x = (width - logo_w) / 2;
        const int logo_y = center_y - (logo_h / 2) - 15;

        auto logo = make_shared<ImageLabel>(Image("file:assets/image/Lice-logo.png"));
        logo->resize(Size(logo_w, logo_h));
        logo->move(Point(logo_x, logo_y));
        container->add(logo);
    }
    catch (...)
    {
        const int logo_w = 120, logo_h = 90;
        const int logo_x = (width - logo_w) / 2;
        const int logo_y = center_y - (logo_h / 2) - 15;

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

    // Texto (basado en nuevo centro)
    const int text_y = center_y + 40;
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

    // Timers (usar constructor con intervalo)
    auto progress_timer  = make_shared<PeriodicTimer>(chrono::milliseconds(16));  // ~60 FPS
    auto initial_check   = make_shared<PeriodicTimer>(chrono::seconds(1));
    auto success_timer   = make_shared<PeriodicTimer>(chrono::seconds(3));
    auto failure_timer   = make_shared<PeriodicTimer>(chrono::seconds(6)); // placeholder
    auto setup_req_timer = make_shared<PeriodicTimer>(chrono::seconds(6)); // placeholder

    // Capturas débiles
    weak_ptr<ProgressCircle> w_progress_circle = progress_circle;
    weak_ptr<Label>          w_subtitle        = subtitle;
    weak_ptr<Widget>         w_container       = container;

    // Animación fluida (loop de 0..100%)
    auto anim_start = make_shared<chrono::steady_clock::time_point>(chrono::steady_clock::now());
    constexpr float percent_per_ms = 100.f / 3000.f; // vuelta en 3s

    progress_timer->on_timeout([=]() {
        if (*connection_complete) return;
        // Si el container fue destruido, cancelar para evitar segfault.
        if (!w_container.lock())
        {
            progress_timer->cancel();
            return;
        }
        auto pc = w_progress_circle.lock();
        if (!pc) return;

        auto elapsed_ms = chrono::duration_cast<chrono::milliseconds>(
                              chrono::steady_clock::now() - *anim_start).count();
        float cycle = fmod(elapsed_ms * percent_per_ms, 100.f);
        // (Opcional) rampa suave inicial (primer 8%)
        constexpr float RAMP = 0.08f;
        float norm = cycle / 100.f;
        if (norm < RAMP)
        {
            float t = norm / RAMP;
            norm = (t * t * (2.f - t)) * RAMP; // easeOutQuad re-escalado
            cycle = norm * 100.f;
        }
        pc->progress(cycle);
    });

    // Helper cancelar timers
    auto cancel_all_timers = [=]() {
        progress_timer->cancel();
        initial_check->cancel();
        success_timer->cancel();
        failure_timer->cancel();
        setup_req_timer->cancel();
    };

    auto handle_done = [=](const char* msg, const Color& c) {
        if (*connection_complete) return;
        *connection_complete = true;
        cancel_all_timers();
        if (auto sub = w_subtitle.lock())
        {
            sub->text(msg);
            sub->color(Palette::ColorId::label_text, c);
        }
        if (auto pc = w_progress_circle.lock())
            pc->progress(100.f);
    };

    // Inicial (una sola vez)
    initial_check->on_timeout([=]() {
        initial_check->cancel();

        if (auto sub = w_subtitle.lock())
            sub->text("Connecting to Wifi");

        *anim_start = chrono::steady_clock::now();
        if (!*connection_complete)
            progress_timer->start();

        // Simular éxito (puedes sustituir por lógica real)
        success_timer->on_timeout([=]() {
            success_timer->cancel();
            handle_done("Wi-Fi Connected", Color(76, 175, 80));
            if (on_connection_complete) on_connection_complete();
        });
        success_timer->start();
    });
    initial_check->start();

    return container;
}
