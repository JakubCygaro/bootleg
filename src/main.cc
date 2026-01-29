#include "bootleg/game.hpp"
#include "defer.hpp"
#include <raylib.h>
#include <stdexcept>

#define WIDTH 800
#define HEIGHT 600

int main(void)
{
    InitWindow(WIDTH, HEIGHT, "Bootleg");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    auto game = boot::Game {WIDTH, HEIGHT};
    DEFER(CloseWindow());
    SetTargetFPS(60);
#ifdef DEBUG
    SetTraceLogLevel(LOG_DEBUG);
#endif
    try{
        game.init();
        while (!WindowShouldClose()) {
            game.update();
            game.draw();
        }
        game.deinit();
    } catch (const std::runtime_error& re){
        TraceLog(LOG_FATAL, "The game encountered an unrecoverable error:\n%s", re.what());
    }
}
