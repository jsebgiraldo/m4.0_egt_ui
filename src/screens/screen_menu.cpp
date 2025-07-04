#include <egt/ui>
#include "screen_menu.h"

using namespace egt;
using namespace std;

shared_ptr<Widget> create_menu_screen(function<void()> on_monitor,
                                      function<void()> on_settings,
                                      function<void()> on_info,
                                      function<void()> on_exit)
{
    const int screen_width = 800;
    const int screen_height = 400;

    auto container = make_shared<Frame>(Rect(0, 0, screen_width, screen_height));

    // Título centrado
    auto title = make_shared<Label>("Main Menu", Rect(0, 20, screen_width, 40));
    title->align(AlignFlag::center_horizontal);
    container->add(title);

    // Grid de 2x2 para los botones principales
    auto grid = make_shared<SelectableGrid>(StaticGrid::GridSize(3, 1));
    grid->resize(Size(320, 120));
    grid->move(Point((screen_width - 320) / 2, 80));
    grid->margin(10);
    grid->horizontal_space(10);
    grid->vertical_space(10);

    // Botones
    auto btn_monitor = make_shared<Button>("Monitor");
    btn_monitor->on_click([=](Event&) { on_monitor(); });
    grid->add(expand(btn_monitor), StaticGrid::GridPoint(0, 0));

    auto btn_settings = make_shared<Button>("Settings");
    btn_settings->on_click([=](Event&) { on_settings(); });
    grid->add(expand(btn_settings), StaticGrid::GridPoint(1, 0));

    auto btn_info = make_shared<Button>("Info");
    btn_info->on_click([=](Event&) { on_info(); });
    grid->add(expand(btn_info), StaticGrid::GridPoint(2, 0));

    container->add(grid);

    // Botón Exit en esquina inferior derecha
    const int exit_width = 100;
    const int exit_height = 40;
    const int exit_x = screen_width - exit_width - 20;
    const int exit_y = screen_height - exit_height - 10;

    auto btn_exit = make_shared<Button>("Exit", Rect(exit_x, exit_y, exit_width, exit_height));
    btn_exit->margin(5);
    btn_exit->on_click([=](Event&) { on_exit(); });
    container->add(btn_exit);

    return container;
}