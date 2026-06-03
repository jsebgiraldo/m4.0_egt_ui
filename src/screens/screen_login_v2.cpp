#include <egt/ui>
#include <chrono>
#include <cmath>
#include <memory>
#include <vector>
#include "screen_login_v2.h"
#include "screen_password_prompt.h"
#include "../ui/components.h"
#include "../ui/design_tokens.h"
#include "../ui/palette.h"

using namespace egt;
using namespace std;

// Figma node 4008:819 — vertical wheel-list picker of technicians.
// Same pattern as the age-step picker in screen_patient_info: fixed slot
// frames + an invisible vertical Slider on top driving the wheel index.
// Tapping a slot opens the password prompt for that technician.

namespace {

// Vertical gradient backdrop for the wheel: medium gray at the top edge,
// near-white in the middle band, medium gray at the bottom. Same trick
// Figma uses to fake the "cylinder rolling behind a curved window" look.
class WheelGradient : public egt::Widget {
public:
    WheelGradient(const egt::Rect& r, float plateau_top, float plateau_bot,
                  float radius = 6.0f)
        : egt::Widget(r), m_radius(radius),
          m_plateau_top(plateau_top), m_plateau_bot(plateau_bot)
    {
        fill_flags({egt::Theme::FillFlag::blend});
        border(0);
    }

    void draw(egt::Painter& painter, const egt::Rect&) override
    {
        auto b = content_area();
        const float x = static_cast<float>(b.x());
        const float y = static_cast<float>(b.y());
        const float w = static_cast<float>(b.width());
        const float h = static_cast<float>(b.height());

        // Colours sampled from the Figma export (node 4008:819):
        //   edge rgb(212,212,212) -> pure white plateau over the centre
        //   slot -> back to rgb(212,212,212). The plateau is what makes the
        //   selected name blend with the gradient instead of fighting it.
        const egt::Color edge {212, 212, 212};
        const egt::Color white{255, 255, 255};
        egt::Pattern grad(egt::Pattern::StepArray{
            {0.0f,            edge},
            {m_plateau_top,   white},
            {m_plateau_bot,   white},
            {1.0f,            edge}
        }, egt::Point(static_cast<int>(x), static_cast<int>(y)),
           egt::Point(static_cast<int>(x), static_cast<int>(y + h)));

        // Draw a rounded rectangle path then fill with the gradient.
        const float r = m_radius;
        const auto PI = static_cast<float>(M_PI);
        painter.draw(egt::PointF(x + r, y));
        painter.line(egt::PointF(x + w - r, y));
        painter.draw(egt::Arc(egt::PointF(x + w - r, y + r),       r, -PI / 2, 0.0f));
        painter.line(egt::PointF(x + w, y + h - r));
        painter.draw(egt::Arc(egt::PointF(x + w - r, y + h - r),   r, 0.0f,   PI / 2));
        painter.line(egt::PointF(x + r, y + h));
        painter.draw(egt::Arc(egt::PointF(x + r, y + h - r),       r, PI / 2, PI));
        painter.line(egt::PointF(x, y + r));
        painter.draw(egt::Arc(egt::PointF(x + r, y + r),           r, PI, 3 * PI / 2));
        painter.set(grad);
        painter.fill();
    }

private:
    float m_radius;
    float m_plateau_top;
    float m_plateau_bot;
};

} // namespace

shared_ptr<Widget> create_login_screen_v2(
    const vector<TechnicianProfile>& technicians,
    function<void(const string& technician_name)> on_login_success,
    function<void()> on_back,
    function<void(shared_ptr<Widget>)> on_show_screen)
{
    auto container = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
    container->fill_flags({Theme::FillFlag::blend});
    container->color(Palette::ColorId::bg, dt::kBgWhite);

    // ── Logo + title ─────────────────────────────────────────────────────
    auto logo = ui::create_logo(4, 7, dt::LOGO_W, dt::LOGO_H);
    container->add(logo);

    auto title = make_shared<Label>("Technician Log-in",
        Rect(0, 28, dt::SCREEN_W, 30), AlignFlag::center);
    title->font(Font(22, Font::Weight::bold));
    title->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(title);

    // ── Wheel-list picker ────────────────────────────────────────────────
    // Six visible slots with the third slot from the top (centre_idx = 2)
    // as the selected row, matching Figma node 4008:819.
    // Dimensions come from sampling the Figma render at scale=2: the picker
    // is 178 figma px wide -> 329 device px, and 165 figma px tall ->
    // 306 device px. With slot_h = 44 (each row is 24 figma px), the
    // chevron + padding band is (306 - 6*44) / 2 = 21 device px per side.
    const int slot_h     = 44;
    const int n_slots    = 6;
    const int chevron_h  = 16;
    const int padding    = 5;
    const int box_w      = 329;
    const int box_h      = n_slots * slot_h + 2 * (chevron_h + padding);
    const int box_x      = (dt::SCREEN_W - box_w) / 2;
    // box_y from Figma: picker's absolute device y = 82 (picker top frame-
    // local y=44 -> 82 device); previously was 66 which made the picker sit
    // too high and pushed the Guest button gap wider than the design.
    const int box_y      = 82;
    const int center_idx = 2;             // third slot from top (0-indexed)
    const int slots_top  = chevron_h + padding;

    // Backdrop is a vertical gradient with a pure-white plateau covering the
    // selected slot - same composition Figma uses. Plateau fractions are
    // computed from the slot geometry so the gradient peak lands exactly on
    // the centre row no matter how slot_h / n_slots are tuned.
    const int sel_top_y = slots_top + center_idx * slot_h;
    const float plateau_top = static_cast<float>(sel_top_y) / box_h;
    const float plateau_bot = static_cast<float>(sel_top_y + slot_h) / box_h;
    auto picker_bg = make_shared<WheelGradient>(
        Rect(box_x, box_y, box_w, box_h),
        plateau_top, plateau_bot, 6.0f);
    container->add(picker_bg);

    // Inner Frame holds the chevrons + slots; transparent so the gradient
    // shows through. No separate white card on the selected slot - the
    // gradient plateau already paints it pure white at the right spot.
    auto picker_box = make_shared<Frame>(Rect(box_x, box_y, box_w, box_h));
    picker_box->fill_flags({});
    picker_box->border(0);
    container->add(picker_box);

    // Up / down arrows from Figma (Polygon 9 + Polygon 10 = nodes 4008:840
    // and 4008:841). Wide flat triangles, not the equilateral Unicode glyph.
    // PNGs are 29x16 (scale=2 of figma 14.5x8), natural device size ~27x15.
    auto load_arrow = [&](const std::string& png_path, int y) {
        auto wrap = make_shared<Frame>(Rect(0, y, box_w, 16));
        wrap->fill_flags({});
        try {
            auto img = Image(("file:" + png_path).c_str());
            auto lbl = make_shared<ImageLabel>(img);
            lbl->autoresize(false);
            lbl->border(0); lbl->padding(0); lbl->margin(0);
            lbl->fill_flags({});
            lbl->image_align(AlignFlag::center);
            lbl->box(Rect(0, 0, box_w, 16));
            wrap->add(lbl);
        } catch (const std::exception& e) {
            printf("[LOGIN] arrow %s missing: %s\n", png_path.c_str(), e.what());
            fflush(stdout);
        }
        return wrap;
    };
    picker_box->add(load_arrow(ui::asset_path("wheel-arrow-up"), 4));
    picker_box->add(load_arrow(ui::asset_path("wheel-arrow-down"),
                               box_h - 16 - 4));

    // The selected index is shared between the slider driver, the slot
    // redraw closure, and the slot-click handlers (so a tap can both
    // recenter the wheel AND open the password prompt for that name).
    int initial_sel = std::min<int>(center_idx,
                                    static_cast<int>(technicians.size()) - 1);
    if (initial_sel < 0) initial_sel = 0;
    auto sel = make_shared<int>(initial_sel);

    // Fixed slot frames — they never move; the visible label text/style
    // is rewritten on every value change.
    struct SlotW {
        shared_ptr<Frame> frame;
        shared_ptr<Label> label;
    };
    auto slots = make_shared<vector<SlotW>>();
    for (int k = 0; k < n_slots; k++) {
        const int slot_y = slots_top + k * slot_h;
        auto rf = make_shared<Frame>(Rect(0, slot_y, box_w, slot_h));
        rf->fill_flags({Theme::FillFlag::blend});
        rf->color(Palette::ColorId::bg, dt::kTransparent);
        rf->border(0);

        auto lbl = make_shared<Label>("",
            Rect(0, 0, box_w, slot_h), AlignFlag::center);
        rf->add(lbl);
        picker_box->add(rf);

        slots->push_back({rf, lbl});
    }

    // Forward-declare the "open password for tech N" handler so the slot
    // click closures and the slider driver can both reach it.
    auto open_password = make_shared<function<void(int)>>();

    auto redraw = [=](int state) {
        const int n = static_cast<int>(technicians.size());
        for (int k = 0; k < n_slots; k++) {
            const int idx = state + (k - center_idx);
            auto& s = (*slots)[k];
            if (idx < 0 || idx >= n) {
                s.label->text("");
                continue;
            }
            const bool is_sel = (k == center_idx);
            s.label->text(technicians[idx].name);
            s.label->font(Font(
                is_sel ? 24 : 19,
                is_sel ? Font::Weight::bold : Font::Weight::normal));
            // Per-slot alpha so the top and bottom items dissolve into the
            // wheel gradient (matches Figma). Alphas chosen by row index, not
            // by distance from centre, since the rows are not symmetric
            // around centre_idx = 2 (we have 2 rows above, 3 below).
            //   k=0 (top edge):    very faint
            //   k=1:               half visible
            //   k=2 (selected):    full alpha, bold
            //   k=3, k=4:          half visible
            //   k=5 (bottom edge): very faint
            int alpha;
            switch (k) {
                case 0:  alpha =  60; break;
                case 1:  alpha = 180; break;
                case 2:  alpha = 255; break;
                case 3:  alpha = 200; break;
                case 4:  alpha = 130; break;
                default: alpha =  60; break;   // k == 5
            }
            const auto base = dt::kTextPrimary;
            const Color c(base.red(), base.green(), base.blue(),
                          static_cast<uint8_t>(alpha));
            s.label->color(Palette::ColorId::label_text, c);
        }
        picker_box->damage();
    };

    redraw(*sel);

    // Slot click: if it's the centre slot, commit (open password); otherwise
    // re-centre the wheel on the tapped name. Two-tap pattern for non-centre
    // slots, single-tap for the highlighted one — matches the wheel-picker
    // convention without needing a separate Login button.
    for (int k = 0; k < n_slots; k++) {
        auto& s = (*slots)[k];
        const int slot_k = k;
        s.frame->on_event([=](Event&) {
            const int n   = static_cast<int>(technicians.size());
            const int idx = *sel + (slot_k - center_idx);
            if (idx < 0 || idx >= n) return;
            if (slot_k == center_idx) {
                if (*open_password) (*open_password)(idx);
            } else {
                *sel = idx;
                redraw(*sel);
            }
        }, {EventId::pointer_click});
    }

    // Invisible vertical Slider drives the wheel during drag — same trick
    // as the age picker and brightness bar. live_update so we get a
    // value-change every pixel of motion.
    auto picker_slider = make_shared<Slider>(
        Rect(box_x, box_y + slots_top, box_w, n_slots * slot_h),
        0, static_cast<int>(technicians.size()) - 1, *sel,
        Orientation::vertical);
    picker_slider->live_update(true);
    picker_slider->fill_flags({});
    picker_slider->border(0);
    for (auto group : {Palette::GroupId::normal,   Palette::GroupId::active,
                       Palette::GroupId::disabled, Palette::GroupId::checked}) {
        picker_slider->color(Palette::ColorId::button_bg, dt::kTransparent, group);
        picker_slider->color(Palette::ColorId::button_fg, dt::kTransparent, group);
        picker_slider->color(Palette::ColorId::border,    dt::kTransparent, group);
        picker_slider->color(Palette::ColorId::bg,        dt::kTransparent, group);
    }
    container->add(picker_slider);

    picker_slider->on_value_changed([=]() {
        const int v = picker_slider->value();
        if (v != *sel) {
            *sel = v;
            redraw(v);
        }
    });

    // The slider sits ON TOP of the slot frames (added after picker_box) so
    // its pointer_drag handler can run — but Slider also consumes
    // pointer_click without firing value_changed, which is why tapping a
    // technician name didn't do anything. Restore tap-to-select by handling
    // pointer_click on the slider itself: compute which slot the user tapped
    // and run the same logic as the (now-unreachable) slot click handlers.
    picker_slider->on_event([=](Event& e) {
        if (e.id() != EventId::pointer_click) return;
        // Slider's box top in display coords is (box_y + slots_top). Subtract
        // to get y inside the slider, then divide by slot_h for the row index.
        const int local_y =
            static_cast<int>(e.pointer().point.y()) - (box_y + slots_top);
        const int k = local_y / slot_h;
        if (k < 0 || k >= n_slots) return;
        const int n   = static_cast<int>(technicians.size());
        const int idx = *sel + (k - center_idx);
        if (idx < 0 || idx >= n) return;
        if (k == center_idx) {
            if (*open_password) (*open_password)(idx);
        } else {
            *sel = idx;
            picker_slider->value(idx);   // keep slider in sync with new centre
            redraw(idx);
        }
    });

    // ── Password flow ────────────────────────────────────────────────────
    // Re-creates the login screen on Back / wrong password so the user
    // returns to the same wheel (state isn't preserved across re-creation
    // — fine for this flow since the wheel re-snaps to centre).
    auto rebuild_login = [=]() {
        if (!on_show_screen) return;
        on_show_screen(create_login_screen_v2(
            technicians, on_login_success, on_back, on_show_screen));
    };

    *open_password = [=](int idx) {
        const auto& tech = technicians[idx];
        if (!on_show_screen) return;
        auto pwd_screen = create_password_prompt_screen(
            "Enter Password",
            "User: " + tech.name,
            "Login",
            "Back",
            [=](const string& entered_password) {
                if (entered_password == tech.password) {
                    printf("Login successful for %s\n", tech.name.c_str());
                    if (on_login_success) on_login_success(tech.name);
                } else {
                    printf("Incorrect password for %s\n", tech.name.c_str());
                    auto err_screen = make_shared<Frame>(
                        Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
                    err_screen->color(Palette::ColorId::bg, dt::kBgWhite);

                    auto err_msg = make_shared<Label>(
                        "Incorrect password!\nPlease try again.",
                        Rect(0, dt::SCREEN_H / 2 - 40, dt::SCREEN_W, 80));
                    err_msg->font(dt::fontTitle());
                    err_msg->color(Palette::ColorId::label_text, dt::kRed);
                    err_screen->add(err_msg);
                    on_show_screen(err_screen);

                    auto timer = make_shared<PeriodicTimer>(
                        chrono::milliseconds(1500));
                    timer->on_timeout([=]() {
                        timer->cancel();
                        rebuild_login();
                    });
                    timer->start();
                }
            },
            rebuild_login);
        on_show_screen(pwd_screen);
    };

    // ── Guest button ─────────────────────────────────────────────────────
    // Same width and x-position as the picker box. With box_y = 82 the
    // picker bottom lands at 388, and Figma places Guest at device y = 398
    // - a 10 px gap below the picker (not 26; that was the artefact of
    // the old box_y = 66).
    const int guest_y = box_y + box_h + 10;
    auto guest = make_shared<Frame>(Rect(box_x, guest_y, box_w, 44));
    guest->fill_flags({Theme::FillFlag::blend});
    guest->color(Palette::ColorId::bg, dt::kGrayBg);
    guest->border(1);
    guest->color(Palette::ColorId::border, dt::kGrayLight);
    guest->border_radius(6);
    container->add(guest);

    auto guest_lbl = make_shared<Label>("Guest",
        Rect(0, 0, box_w, 44), AlignFlag::center);
    // Figma 4008:833: fontSize 11 Medium (weight 500) -> device 20 pt Normal.
    guest_lbl->font(Font("Gothic A1", 20, Font::Weight::normal));
    guest_lbl->color(Palette::ColorId::label_text, dt::kTextPrimary);
    guest->add(guest_lbl);

    guest->on_event([=](Event&) {
        if (on_login_success) on_login_success("Guest");
    }, {EventId::pointer_click});

    // ── Back (shared chevron + label at standard position) ───────────────
    ui::add_back_button(*container, on_back);

    return container;
}
