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
    // Layout constants chosen so the box visually matches Figma's 180×166
    // ratio scaled up to a 800×480 viewport (≈ 380×320 here). Slot_h is
    // bumped to 52 so the names breathe; the box stretches down to give
    // Guest room to sit comfortably above Back without crowding.
    const int slot_h     = 52;
    const int n_slots    = 5;             // odd → middle slot is "selected"
    const int chevron_h  = 22;
    const int padding    = 8;
    const int box_w      = 380;
    const int box_h      = n_slots * slot_h + 2 * (chevron_h + padding);
    const int box_x      = (dt::SCREEN_W - box_w) / 2;
    const int box_y      = 66;
    const int center_idx = n_slots / 2;
    const int slots_top  = chevron_h + padding;

    auto picker_box = make_shared<Frame>(Rect(box_x, box_y, box_w, box_h));
    picker_box->fill_flags({Theme::FillFlag::blend});
    picker_box->color(Palette::ColorId::bg, dt::kGrayBg);
    picker_box->border(0);
    picker_box->border_radius(6);
    container->add(picker_box);

    auto up_arrow = make_shared<Label>(u8"▲",
        Rect(0, 4, box_w, chevron_h), AlignFlag::center);
    up_arrow->font(Font(16));
    up_arrow->color(Palette::ColorId::label_text, palette::kGray400);
    picker_box->add(up_arrow);

    auto down_arrow = make_shared<Label>(u8"▼",
        Rect(0, box_h - chevron_h - 4, box_w, chevron_h), AlignFlag::center);
    down_arrow->font(Font(16));
    down_arrow->color(Palette::ColorId::label_text, palette::kGray400);
    picker_box->add(down_arrow);

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
            const int  dist   = std::abs(k - center_idx);
            s.label->text(technicians[idx].name);
            s.label->font(Font(
                is_sel ? 24 : 19,
                is_sel ? Font::Weight::bold : Font::Weight::normal));
            const Color c =
                is_sel        ? dt::kTextPrimary
              : (dist == 1)   ? palette::kGray500
                              : palette::kGray400;
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
    // Centered below the wheel, same width as the picker box.
    const int guest_y = box_y + box_h + 14;
    auto guest = make_shared<Frame>(Rect(box_x, guest_y, box_w, 50));
    guest->fill_flags({Theme::FillFlag::blend});
    guest->color(Palette::ColorId::bg, dt::kGrayBg);
    guest->border(1);
    guest->color(Palette::ColorId::border, dt::kGrayLight);
    guest->border_radius(6);
    container->add(guest);

    auto guest_lbl = make_shared<Label>("Guest",
        Rect(0, 0, box_w, 50), AlignFlag::center);
    guest_lbl->font(Font(18, Font::Weight::bold));
    guest_lbl->color(Palette::ColorId::label_text, dt::kTextPrimary);
    guest->add(guest_lbl);

    guest->on_event([=](Event&) {
        if (on_login_success) on_login_success("Guest");
    }, {EventId::pointer_click});

    // ── Back (shared chevron + label at standard position) ───────────────
    ui::add_back_button(*container, on_back);

    return container;
}
