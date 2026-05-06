#include <egt/ui>
#include "screen_settings.h"
#include "../ui/components.h"
#include "../ui/design_tokens.h"
#include "../ui/palette.h"
#include "../ui/brightness.h"

using namespace egt;
using namespace std;

shared_ptr<Widget> create_settings_screen(
    function<void()> on_back,
    function<void()> on_wifi_settings)
{
    auto container = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
    container->fill_flags({Theme::FillFlag::blend});
    container->color(Palette::ColorId::bg, dt::kBgWhite);

    // Logo
    auto logo = ui::create_logo(10, 5, dt::LOGO_W, dt::LOGO_H);
    container->add(logo);

    // Title
    auto title = make_shared<Label>("Settings",
        Rect(dt::LOGO_W + 30, 30, 300, 40));
    title->font(dt::fontTitle());
    title->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(title);

    // Separator
    auto sep = make_shared<Frame>(Rect(0, 115, dt::SCREEN_W, 2));
    sep->fill_flags({Theme::FillFlag::blend});
    sep->color(Palette::ColorId::bg, dt::kGrayLight);
    sep->border(0);
    container->add(sep);

    // ── WiFi row ────────────────────────────────────────────────────────────
    auto wifi_row = make_shared<Frame>(Rect(30, 130, dt::SCREEN_W - 60, 78));
    wifi_row->fill_flags({Theme::FillFlag::blend});
    wifi_row->color(Palette::ColorId::bg, dt::kGrayBg);
    wifi_row->border(0);
    wifi_row->border_radius(dt::RADIUS_LG);
    container->add(wifi_row);

    auto wifi_lbl = make_shared<Label>("Wi-Fi",
        Rect(20, 0, 200, 70));
    wifi_lbl->font(Font(20, Font::Weight::bold));
    wifi_lbl->color(Palette::ColorId::label_text, dt::kTextPrimary);
    wifi_lbl->text_align(AlignFlag::left | AlignFlag::center_vertical);
    wifi_row->add(wifi_lbl);

    auto wifi_chev_bg = make_shared<Frame>(Rect(dt::SCREEN_W - 132, 20, 34, 34));
    wifi_chev_bg->fill_flags({Theme::FillFlag::blend});
    wifi_chev_bg->color(Palette::ColorId::bg, dt::kWhite);
    wifi_chev_bg->border(1);
    wifi_chev_bg->color(Palette::ColorId::border, dt::kGrayLight);
    wifi_chev_bg->border_radius(17);
    wifi_row->add(wifi_chev_bg);

    auto wifi_chevron = make_shared<Label>(">",
        Rect(dt::SCREEN_W - 132, 20, 34, 34));
    wifi_chevron->font(Font(18, Font::Weight::bold));
    wifi_chevron->color(Palette::ColorId::label_text, dt::kTextPrimary);
    wifi_row->add(wifi_chevron);

    wifi_row->on_event([on_wifi_settings](Event&) {
        if (on_wifi_settings) on_wifi_settings();
    }, {EventId::pointer_click});

    // ── Brightness row ──────────────────────────────────────────────────────
    auto bright_row = make_shared<Frame>(Rect(30, 223, dt::SCREEN_W - 60, 170));
    bright_row->fill_flags({Theme::FillFlag::blend});
    bright_row->color(Palette::ColorId::bg, dt::kGrayBg);
    bright_row->border(0);
    bright_row->border_radius(dt::RADIUS_LG);
    container->add(bright_row);

    auto bright_lbl = make_shared<Label>("Brightness",
        Rect(20, 8, 200, 30));
    bright_lbl->font(Font(20, Font::Weight::bold));
    bright_lbl->color(Palette::ColorId::label_text, dt::kTextPrimary);
    bright_lbl->text_align(AlignFlag::left);
    bright_row->add(bright_lbl);

    auto bright_hint = make_shared<Label>("0% keeps screen visible for safety",
        Rect(20, 38, 360, 22));
    bright_hint->font(Font(14, Font::Weight::normal));
    bright_hint->color(Palette::ColorId::label_text, palette::kGray600);
    bright_hint->text_align(AlignFlag::left);
    bright_row->add(bright_hint);

    // Current brightness value label
    int cur_brightness = ui::get_brightness();
    if (cur_brightness < 0) cur_brightness = 80; // default
    auto bright_val_pill = make_shared<Frame>(Rect(dt::SCREEN_W - 165, 16, 70, 34));
    bright_val_pill->fill_flags({Theme::FillFlag::blend});
    bright_val_pill->color(Palette::ColorId::bg, dt::kWhite);
    bright_val_pill->border(0);
    bright_val_pill->border_radius(17);
    bright_row->add(bright_val_pill);

    auto bright_val = make_shared<Label>(to_string(cur_brightness) + "%",
        Rect(dt::SCREEN_W - 165, 16, 70, 34));
    bright_val->font(Font(20, Font::Weight::bold));
    bright_val->color(Palette::ColorId::label_text, dt::kGreen);
    bright_val->text_align(AlignFlag::center);
    bright_row->add(bright_val);

    // Brightness buttons: -, track, +
    const int btn_sz = 54;
    const int track_x = 80;
    const int track_w = dt::SCREEN_W - 60 - track_x - 88;
    const int btn_y = 88;

    auto btn_minus = make_shared<Button>("-", Rect(20, btn_y, btn_sz, btn_sz));
    btn_minus->font(Font(24, Font::Weight::bold));
    btn_minus->color(Palette::ColorId::button_bg, dt::kWhite);
    btn_minus->color(Palette::ColorId::button_text, dt::kTextPrimary);
    btn_minus->border(1);
    btn_minus->color(Palette::ColorId::border, dt::kGrayLight);
    btn_minus->border_radius(btn_sz / 2);
    bright_row->add(btn_minus);

    auto btn_plus = make_shared<Button>("+", Rect(track_x + track_w + 10, btn_y, btn_sz, btn_sz));
    btn_plus->font(Font(24, Font::Weight::bold));
    btn_plus->color(Palette::ColorId::button_bg, dt::kWhite);
    btn_plus->color(Palette::ColorId::button_text, dt::kTextPrimary);
    btn_plus->border(1);
    btn_plus->color(Palette::ColorId::border, dt::kGrayLight);
    btn_plus->border_radius(btn_sz / 2);
    bright_row->add(btn_plus);

    auto track_tip = make_shared<Label>("Drag handle for real-time adjustment",
        Rect(track_x, btn_y + 44, track_w, 22));
    track_tip->font(Font(14, Font::Weight::normal));
    track_tip->color(Palette::ColorId::label_text, palette::kGray600);
    track_tip->text_align(AlignFlag::center);
    bright_row->add(track_tip);

    auto slider = make_shared<Slider>(Rect(track_x, btn_y + 14, track_w, 28), 0, 100, cur_brightness);
    slider->slider_flags().set(Slider::SliderFlag::round_handle);
    // Handle: green fill, soft shadow border
    slider->color(Palette::ColorId::button_bg, palette::kSliderHandle);
    slider->color(Palette::ColorId::border, palette::kSliderHandleBorder);
    slider->border(2);
    // Track: green active, gray inactive
    slider->color(Palette::ColorId::button_fg, palette::kSliderTrackActive);
    slider->color(Palette::ColorId::button_fg, palette::kSliderTrackInactive, Palette::GroupId::disabled);
    slider->live_update(true);
    bright_row->add(slider);

    // Dynamic handle color on touch (pressed = darker green)
    slider->on_event([=](Event&) {
        slider->color(Palette::ColorId::button_bg, palette::kSliderHandlePressed);
        slider->damage();
    }, {EventId::pointer_drag_start});

    slider->on_event([=](Event&) {
        slider->color(Palette::ColorId::button_bg, palette::kSliderHandle);
        slider->damage();
    }, {EventId::pointer_drag_stop});

    auto apply_brightness = [=](int value) {
        int v = max(0, min(100, value));
        ui::set_brightness(v);
        bright_val->text(to_string(v) + "%");
        bright_row->damage();
    };

    slider->on_value_changed([=]() {
        apply_brightness(slider->value());
    });

    btn_minus->on_click([=](Event&) {
        int v = max(0, slider->value() - 10);
        slider->value(v);
        apply_brightness(v);
    });

    btn_plus->on_click([=](Event&) {
        int v = min(100, slider->value() + 10);
        slider->value(v);
        apply_brightness(v);
    });

    // ── Back button ─────────────────────────────────────────────────────────
    auto btn_back = ui::create_outlined_button("Back",
        Rect(30, dt::SCREEN_H - 80, 200, 60),
        on_back);
    container->add(btn_back);

    return container;
}
