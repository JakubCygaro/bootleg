#include "bootleg/editor_window.hpp"
#include "bootleg/game.hpp"
#include "defer.hpp"
#include <raylib.h>
#include <memory>


int main(void)
{
    InitWindow(800, 600, "bed");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    auto font = GetFontDefault();
    auto game = boot::Game {font};
    DEFER(CloseWindow());
    DEFER(
        if (font.texture.id != GetFontDefault().texture.id)
            UnloadFont(font););
    SetTargetFPS(60);
    auto w = std::make_unique<boot::EditorWindow>(boot::EditorWindow(font, { 0, 0, 800., 600. }));
    game.windows.push_back(std::move(w));
    while (!WindowShouldClose()) {
        game.update();
        game.draw();
    }
}
