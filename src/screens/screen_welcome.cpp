#include <egt/ui>
#include "screen_welcome.h"

using namespace egt;
using namespace std;

shared_ptr<Widget> create_welcome_screen(function<void()> on_next)
{
    auto container = make_shared<Frame>(Rect(0, 0, 800, 480));

    // Título
    auto title = make_shared<Label>("Welcome to EGT!", Rect(100, 40, 280, 40));
    title->align(AlignFlag::center_horizontal);
    container->add(title);

    // Subtítulo
    auto subtitle = make_shared<Label>("Your custom app starts here", Rect(60, 90, 360, 30));
    subtitle->align(AlignFlag::center_horizontal);
    container->add(subtitle);

    // Botón
    auto btn = make_shared<Button>("Continue", Rect(140, 160, 200, 50));
    btn->align(AlignFlag::center_horizontal);
    btn->on_click([=](Event&) { on_next(); });
    container->add(btn);

    return container;
}
