#include "screen_wifi_network_details.h"
#include <egt/ui>
#include <egt/widget.h>
#include <egt/virtualkeyboard.h>

#include <string>

using namespace egt;
using namespace egt_wifi;

std::shared_ptr<egt::Widget> create_wifi_network_details_screen(
    const egt_wifi::WiFiNetwork& network,
    std::function<void()> on_back,
    std::function<void(const std::string& ssid, const std::string& password)> on_connect
)
{
    auto root = std::make_shared<Window>(Rect(0, 0, 800, 480));
    root->color(Palette::ColorId::bg, Palette::white);
    root->show();

    auto vsizer = std::make_shared<BoxSizer>(Orientation::vertical);
    vsizer->align(AlignFlag::expand_horizontal);
    root->add(vsizer);

    // Título
    auto title = std::make_shared<Label>("Network: " + network.ssid);
    title->font(Font(22, Font::Weight::bold));
    title->align(AlignFlag::center_horizontal);
    vsizer->add(title);

   // Layout para contraseña y botones
    auto form_sizer = std::make_shared<BoxSizer>(Orientation::horizontal);
    form_sizer->align(AlignFlag::expand_horizontal);
    form_sizer->margin(10);

    auto textbox = std::make_shared<TextBox>("");
    textbox->text_align(AlignFlag::left);
    textbox->align(AlignFlag::expand_horizontal);
    textbox->resize(Size(300, 60));
    form_sizer->add(textbox);

    auto btn_join = std::make_shared<Button>("Join");
    btn_join->margin(5);
    btn_join->color(Palette::ColorId::button_bg, Palette::blue);
    //btn_join->align(AlignFlag::expand_horizontal);
    btn_join->on_click([=](Event&) {
        on_connect(network.ssid, textbox->text());
    });
    form_sizer->add(btn_join);

    auto btn_cancel = std::make_shared<Button>("Cancel");
    btn_cancel->margin(5);
    //btn_cancel->align(AlignFlag::expand_horizontal);
    btn_cancel->on_click([on_back](Event&) {
        on_back();
    });
    form_sizer->add(btn_cancel);

    vsizer->add(form_sizer);

    // Teclado virtual debajo
    auto vkeyboard = std::make_shared<VirtualKeyboard>(Rect(20, 150, 760, 300));
    vsizer->add(vkeyboard);

    root->damage();

    return root;
}