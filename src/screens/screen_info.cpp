#include <egt/ui>
#include "screen_info.h"

using namespace egt;
using namespace std;

shared_ptr<Widget> create_info_screen(function<void()> on_back)
{
    auto container = make_shared<Frame>(Rect(0, 0, 800, 480));
    auto label = make_shared<Label>("Info Screen", Rect(100, 40, 200, 40));
    label->align(AlignFlag::center_horizontal);
    container->add(label);

    auto btn = make_shared<Button>("Back", Rect(100, 120, 200, 40));
    btn->align(AlignFlag::center_horizontal);
    btn->on_click([=](Event&) { on_back(); });
    container->add(btn);

    return container;
}