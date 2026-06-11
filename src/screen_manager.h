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
        // Almost every navigation is triggered from a click handler living
        // on the OUTGOING screen. If remove_all() drops the last reference,
        // that widget tree is destroyed while its handler is still on the
        // call stack, and EGT keeps dispatching over freed memory after the
        // handler returns (observed as std::length_error /
        // basic_string::_M_create aborts on the wrong-password path).
        // Park the outgoing screen in m_prev and only release it on the
        // NEXT navigation — by then none of its handlers can be live.
        m_prev    = std::move(m_current);
        m_current = screen;

        m_win.remove_all();
        m_win.add(screen);
        screen->show();
    }

private:
    egt::TopWindow& m_win;
    std::shared_ptr<egt::Widget> m_current;  // screen on display
    std::shared_ptr<egt::Widget> m_prev;     // outgoing screen, kept alive
                                             // until the next navigation
};
