#ifndef GUI_MENUS_MAIN_WINDOW_H
#define GUI_MENUS_MAIN_WINDOW_H

#include <string>
#include <vector>

namespace ui {
    struct State;
}

namespace ui { namespace menus {
    void drawMain(State& ui, bool* p_open);
    void drawGitHub(State& ui);
} }

#endif


