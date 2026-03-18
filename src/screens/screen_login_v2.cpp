#include <egt/ui>
#include <chrono>
#include "screen_login_v2.h"
#include "screen_password_prompt.h"
#include "../ui/components.h"
#include "../ui/design_tokens.h"

using namespace egt;
using namespace std;

// Figma canvas: 432×261, screen: 800×480 → scale ≈ 1.852
// Group 172 origin: -640,272 (all positions relative to this)

// ── Helper: create a technician card ────────────────────────────────────────
// Figma: 143×46 cards, #F4F3F3 bg, selected = #5BC500, text #646569, r=4
// Scaled: 265×85, r=7, font fs=20 fw=bold, smiley icon ~41px
static shared_ptr<Frame> create_tech_card(
    const string& name,
    int x, int y, int w, int h,
    function<void()> on_click)
{
    auto card = make_shared<Frame>(Rect(x, y, w, h));
    card->fill_flags({Theme::FillFlag::blend});
    card->color(Palette::ColorId::bg, dt::kGrayBg);
    card->border(0);
    card->border_radius(7);

    // Smiley icon placeholder (circle, left side of card)
    const int icon_sz = 41;
    const int icon_x = 12;
    const int icon_y = (h - icon_sz) / 2;
    auto icon = make_shared<Frame>(Rect(icon_x, icon_y, icon_sz, icon_sz));
    icon->fill_flags({Theme::FillFlag::blend});
    icon->color(Palette::ColorId::bg, Color(0xD9, 0xD9, 0xD9));
    icon->border(0);
    icon->border_radius(icon_sz / 2);
    card->add(icon);

    // Name label (right of icon)
    auto label = make_shared<Label>(name,
        Rect(icon_x + icon_sz + 8, 0, w - icon_x - icon_sz - 20, h),
        AlignFlag::center_vertical | AlignFlag::left);
    label->font(Font(dt::FONT_BODY, Font::Weight::bold));
    label->color(Palette::ColorId::label_text, dt::kTextPrimary);
    card->add(label);

    if (on_click) {
        card->on_event([on_click](Event& event) {
            if (event.id() == EventId::pointer_click) {
                on_click();
                return true;
            }
            return false;
        });
    }

    return card;
}

// ── Login screen ────────────────────────────────────────────────────────────
shared_ptr<Widget> create_login_screen_v2(
    const vector<TechnicianProfile>& technicians,
    function<void(const string& technician_name)> on_login_success,
    function<void()> on_back,
    function<void(shared_ptr<Widget>)> on_show_screen)
{
    auto container = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
    container->fill_flags({Theme::FillFlag::blend});
    container->color(Palette::ColorId::bg, dt::kBgWhite);

    // Figma: Logo 90×56 @2,4 → scaled 167×104 @4,7
    auto logo = ui::create_logo(4, 7, dt::LOGO_W, dt::LOGO_H);
    container->add(logo);

    // Figma: "Technician Log-in" 100×25 @172,26 → scaled ~319,48, fs=12→22
    auto title = make_shared<Label>("Technician Log-in",
        Rect(319, 48, 250, 46),
        AlignFlag::center_vertical | AlignFlag::left);
    title->font(Font(22, Font::Weight::normal));
    title->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(title);

    // Card grid: Figma layout (scaled)
    // 2 columns, each card 265×85
    // Col 1 x=130, Col 2 x=404
    // Row 1 y=119, Row 2 y=211
    // Guest centered x=267, y=304
    const int card_w = 265;
    const int card_h = 85;
    const int col1_x = 130;
    const int col2_x = 404;
    const int row1_y = 119;
    const int row2_y = 211;
    const int guest_x = 267;
    const int guest_y = 304;

    // Position lookup: [row][col] → {x, y}
    struct Pos { int x, y; };
    Pos positions[] = {
        {col1_x, row1_y}, {col2_x, row1_y},  // row 1
        {col1_x, row2_y}, {col2_x, row2_y},  // row 2
    };

    for (size_t i = 0; i < technicians.size() && i < 4; i++)
    {
        const auto& tech = technicians[i];
        auto card = create_tech_card(tech.name,
            positions[i].x, positions[i].y, card_w, card_h,
            [=]() {
                auto pwd_screen = create_password_prompt_screen(
                    "Enter Password",
                    "User: " + tech.name,
                    "Login",
                    "Back",
                    [=](const string& entered_password) {
                        if (entered_password == tech.password) {
                            printf("Login successful for %s\n", tech.name.c_str());
                            if (on_login_success)
                                on_login_success(tech.name);
                        } else {
                            printf("Incorrect password for %s\n", tech.name.c_str());
                            auto err_screen = make_shared<Frame>(
                                Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
                            err_screen->color(Palette::ColorId::bg, dt::kBgWhite);

                            auto err_msg = make_shared<Label>(
                                "Incorrect password!\nPlease try again.",
                                Rect(0, dt::SCREEN_H / 2 - 40, dt::SCREEN_W, 80));
                            err_msg->align(AlignFlag::center);
                            err_msg->font(dt::fontTitle());
                            err_msg->color(Palette::ColorId::label_text, dt::kRed);
                            err_screen->add(err_msg);

                            if (on_show_screen) on_show_screen(err_screen);

                            auto timer = make_shared<PeriodicTimer>(
                                chrono::milliseconds(1500));
                            timer->on_timeout([=]() {
                                timer->cancel();
                                auto login = create_login_screen_v2(
                                    technicians, on_login_success,
                                    on_back, on_show_screen);
                                if (on_show_screen) on_show_screen(login);
                            });
                            timer->start();
                        }
                    },
                    [=]() {
                        auto login = create_login_screen_v2(
                            technicians, on_login_success,
                            on_back, on_show_screen);
                        if (on_show_screen) on_show_screen(login);
                    }
                );
                if (on_show_screen) on_show_screen(pwd_screen);
            });
        container->add(card);
    }

    // Guest card (centered below the grid)
    auto guest_card = create_tech_card("Guest",
        guest_x, guest_y, card_w, card_h,
        [=]() {
            if (on_login_success) on_login_success("Guest");
        });
    container->add(guest_card);

    // Back button — Figma: 93×33 @14,216 → scaled 172×61 @26,400
    auto btn_back = ui::create_outlined_button(
        "< Back",
        Rect(26, 400, 172, 61),
        on_back);
    container->add(btn_back);

    return container;
}
