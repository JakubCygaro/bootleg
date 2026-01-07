#include "defer.hpp"
#include <algorithm>
#include <print>
#include <raylib.h>

constexpr const int FONT_SIZE = 24;

struct TextBuffer {
    std::string buffer = {};
    long int cursor = 0;
};

static TextBuffer _text_buffer = {};

void update_buffer(void)
{
    int k = 0;
    while ((k = GetKeyPressed())) {
        if (k == KEY_BACKSPACE && !_text_buffer.buffer.empty()) {
            _text_buffer.buffer.erase(--_text_buffer.cursor, 1);
        } else if (k == KEY_ENTER) {
            _text_buffer.buffer.push_back('!');
            std::shift_right(_text_buffer.buffer.begin() + _text_buffer.cursor, _text_buffer.buffer.end(), 1);
            _text_buffer.buffer[_text_buffer.cursor++] = static_cast<char>('\n');
        } else if (k == KEY_TAB) {
            for(auto i = 0; i < 4; i++) {
                _text_buffer.buffer.push_back('!');
                std::shift_right(_text_buffer.buffer.begin() + _text_buffer.cursor, _text_buffer.buffer.end(), 1);
                _text_buffer.buffer[_text_buffer.cursor++] = static_cast<char>(' ');
            }
        } else if (k == KEY_RIGHT) {
            _text_buffer.cursor++;
        } else if (k == KEY_LEFT) {
            _text_buffer.cursor--;
        }
    }
    _text_buffer.cursor = _text_buffer.cursor > _text_buffer.buffer.size() ? _text_buffer.buffer.size() : _text_buffer.cursor;
    _text_buffer.cursor = _text_buffer.cursor < 0 ? 0 : _text_buffer.cursor;
    int c = 0;
    while ((c = GetCharPressed())) {
        _text_buffer.buffer.push_back('!');
        std::shift_right(_text_buffer.buffer.begin() + _text_buffer.cursor, _text_buffer.buffer.end(), 1);
        _text_buffer.buffer[_text_buffer.cursor++] = static_cast<char>(c);
    }
}

void draw_buffer(void)
{
    // DrawText(_text_buffer.buffer.data(), 0, 0, FONT_SIZE, WHITE);
    const Font font = GetFontDefault();
    Vector2 pos = { 0, 0 };
    const float scale_factor = FONT_SIZE / (float)font.baseSize;
    Rectangle last_c_r = { 0 };
    for (std::size_t i = 0; i < _text_buffer.buffer.size(); i++) {
        wchar_t c = _text_buffer.buffer[i];
        int idx = GetGlyphIndex(font, c);
        float glyph_width = (font.glyphs[idx].advanceX == 0) ? font.recs[idx].width * scale_factor : font.glyphs[idx].advanceX * scale_factor;
        float glyph_height = font.recs[idx].height * scale_factor;

        auto r = GetGlyphAtlasRec(font, idx);
        if (i == _text_buffer.cursor) {
            DrawRectangleRec(Rectangle {
                                 .x = pos.x,
                                 .y = pos.y,
                                 .width = 2.,
                                 .height = glyph_height },
                WHITE);
        }
        if (c == '\n') {
            pos.x = 0;
            pos.y += r.height * scale_factor;
        } else {
            DrawTextCodepoint(font, c, pos, 24, WHITE);
            pos.x += glyph_width + 2;
        }
        last_c_r = r;
    }
    if (_text_buffer.cursor == _text_buffer.buffer.size()) {
        DrawRectangleRec(Rectangle {
                             .x = pos.x,
                             .y = pos.y,
                             .width = 2.,
                             .height = font.recs[GetGlyphIndex(font, ' ')].height * scale_factor },
            WHITE);
    }
}

int main(void)
{
    InitWindow(800, 600, "bootleg");
    SetTargetFPS(60);
    _text_buffer.buffer = "Welcome to Bootleg";
    DEFER(CloseWindow());
    while (!WindowShouldClose()) {
        update_buffer();
        BeginDrawing();
        ClearBackground(BLACK);
        draw_buffer();
        EndDrawing();
    }
}
