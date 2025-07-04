#pragma once
#include <egt/ui>
#include <memory>
#include <functional>

class ScreenManager
{
public:
    ScreenManager(egt::TopWindow& win) : m_win(win) {}

    void show(std::shared_ptr<egt::Widget> screen)
    {
        m_win.remove_all();
        m_win.add(screen);
        screen->show();
    }

private:
    egt::TopWindow& m_win;
};
