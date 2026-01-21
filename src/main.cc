#include "bootleg/game.hpp"
#include "defer.hpp"
#include <raylib.h>

#define WIDTH 800
#define HEIGHT 600

int main(void)
{
    InitWindow(WIDTH, HEIGHT, "Bootleg");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    auto game = boot::Game {WIDTH, HEIGHT};
    DEFER(CloseWindow());
    SetTargetFPS(60);
    game.init();
    while (!WindowShouldClose()) {
        game.update();
        game.draw();
    }
    game.deinit();
}
