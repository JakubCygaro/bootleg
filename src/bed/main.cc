#include "defer.hpp"
#include "utf8.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <raylib.h>
#include "buffer.hpp"

// lifted from raylib examples
static void add_codepoints_range(Font* font, const char* fontPath, int start, int stop)
{
    int rangeSize = stop - start + 1;
    int currentRangeSize = font->glyphCount;

    int updatedCodepointCount = currentRangeSize + rangeSize;
    int* updatedCodepoints = new int[updatedCodepointCount];
    DEFER(delete[] updatedCodepoints);

    for (int i = 0; i < currentRangeSize; i++)
        updatedCodepoints[i] = font->glyphs[i].value;

    for (int i = currentRangeSize; i < updatedCodepointCount; i++)
        updatedCodepoints[i] = start + (i - currentRangeSize);

    UnloadFont(*font);
    *font = LoadFontEx(fontPath, 100, updatedCodepoints, updatedCodepointCount);
}
Font try_load_font(char* path)
{
    if (!FileExists(path))
        return GetFontDefault();
    auto font = LoadFontEx(path, 100, NULL, 0);
    add_codepoints_range(&font, path, 0xc0, 0x17f);
    add_codepoints_range(&font, path, 0x180, 0x24f);
    return font;
}
int main(int argc, char** args)
{
    Rectangle bounds = {
        .x = 0,
        .y = 0,
        .width = 800,
        .height = 600,
    };
    InitWindow(800, 600, "bed");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    DEFER(CloseWindow());
    auto font = argc == 2 ? (try_load_font(args[1])) : GetFontDefault();
    DEFER(
        if (font.texture.id != GetFontDefault().texture.id)
            UnloadFont(font););
    TextBuffer _text_buffer = { font, bounds };
    _text_buffer.insert_string("Welcome to Bed!");
    _text_buffer.set_font_size(50);
    SetTargetFPS(60);
    while (!WindowShouldClose()) {
        if(IsWindowResized()){
            _text_buffer.set_width(GetScreenWidth());
            _text_buffer.set_height(GetScreenHeight());
        }
        _text_buffer.update_buffer();
        BeginDrawing();
        ClearBackground(BLACK);
        _text_buffer.draw();
        EndDrawing();
    }
}
