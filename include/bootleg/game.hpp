#ifndef BOOT_GAME_HPP
#define BOOT_GAME_HPP
#include <raylib.h>
#include <vector>
#include <memory>
namespace boot {
class Game;

struct Window {
    virtual void update(Game& game_state) = 0;
    virtual void draw(Game& game_state) = 0;
    virtual const char* get_window_name() = 0;
    inline virtual ~Window(){};
};

class Game {
    Font m_font;
public:
    inline Game(Font font) : m_font(font) {}
    // windows? I'm gonna puke
    std::vector<std::unique_ptr<Window>> windows {};
    void update();
    void draw();
};
}
#endif
