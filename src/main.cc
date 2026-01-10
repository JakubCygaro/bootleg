#include "defer.hpp"
#include <algorithm>
#include <iostream>
#include <optional>
#include <ostream>
#include <print>
#include <raylib.h>

#define CTRL true
#define META true
#define SHIFT true
#define NO false
#define KEYS(k, KEY, C, M, S) (k.key == KEY && k.control == C && k.meta == M && k.shift == S)
#define KEYSDOWN(k) (IsKeyDown(k.key) && (k.control ? IsKeyDown(KEY_LEFT_CONTROL) : true) && (k.))

constexpr const int FONT_SIZE = 24;

struct TextBuffer {
    std::string buffer = {};
    long int cursor = 0;
    void normalize_cursor_position(void)
    {
        cursor = cursor > (long)buffer.size() ? (long)buffer.size() : cursor;
        cursor = cursor < 0 ? 0 : cursor;
    }
    void move_cursor_left(long amount = 1)
    {
        move_cursor(-amount);
    }
    void move_cursor_right(long amount = 1)
    {
        move_cursor(amount);
    }
    void move_cursor(long amount)
    {
        cursor += amount;
    }
    void move_cursor_down(void)
    {
        auto nl = buffer.find('\n', cursor + 1);
        if (nl == std::string::npos){
            return;
        }
        auto back_nl = buffer.rfind('\n', cursor);
        std::flush(std::cout);
        auto dist = cursor - (back_nl == std::string::npos ? -1 : back_nl);
        cursor = nl + dist;
        auto next_nl = buffer.find('\n', cursor);
        cursor = cursor > next_nl ? next_nl : cursor;
    }
    void move_cursor_up(void)
    {
        auto back_nl = buffer.rfind('\n', cursor);
        if (back_nl == std::string::npos)
            return;
        auto dist = cursor - back_nl;
        auto back2_nl = buffer.rfind('\n', back_nl - 1);
        cursor = (back2_nl == std::string::npos ? -1 : back2_nl) + dist;
    }
    void delete_characters(unsigned int amount = 1)
    {
        if (buffer.empty() || cursor <= 0)
            return;
        cursor -= amount;
        buffer.erase(cursor, amount);
    }
    void push_character(char c)
    {
        buffer.push_back('!');
        std::shift_right(buffer.begin() + cursor, buffer.end(), 1);
        buffer[cursor++] = static_cast<char>('c');
    }
    void jump_cursor_to_end(void)
    {
        auto nl = buffer.find('\n', cursor);
        cursor = (nl == std::string::npos ? buffer.size() : nl);
    }
};

static TextBuffer _text_buffer = {};

struct Keys {
    bool control {};
    bool meta {};
    bool shift {};
    int key {};
};

std::optional<Keys> get_keys_pressed(void)
{
    int k;
    std::optional<Keys> ret {};
    while ((k = GetKeyPressed())) {
        ret = ret.has_value() ? ret : Keys {};
        switch (k) {
        case KEY_LEFT_CONTROL:
        case KEY_RIGHT_CONTROL:
            ret.value().control = true;
            break;
        case KEY_LEFT_SHIFT:
        case KEY_RIGHT_SHIFT:
            ret.value().shift = true;
            break;
        case KEY_LEFT_ALT:
        case KEY_RIGHT_ALT:
            ret.value().meta = true;
            break;
        default:
            ret.value().key = k;
            break;
        }
    }
    return ret;
}

void handle_keys_pressed(Keys keys)
{
    static Keys last_keys = {};
    if (KEYS(keys, KEY_RIGHT, NO, NO, NO)) {
        _text_buffer.move_cursor_right();
    }
    if (KEYS(keys, KEY_LEFT, NO, NO, NO)) {
        _text_buffer.move_cursor_left();
    }
    if (KEYS(keys, KEY_UP, NO, NO, NO)) {
        _text_buffer.move_cursor_up();
    }
    if (KEYS(keys, KEY_DOWN, NO, NO, NO)) {
        _text_buffer.move_cursor_down();
    }
    if (KEYS(keys, KEY_BACKSPACE, NO, NO, NO)) {
        _text_buffer.delete_characters();
    }
}

void update_buffer(void)
{
    // if (auto keys = get_keys_pressed(); keys) {
    //     handle_keys_pressed(keys.value());
    //     return;
    // }
    // return;
    const double input_timeout = .08;
    int k = 0;
    static double bs_t = 0;
    const bool input_window = bs_t <= GetTime();
    if (IsKeyDown(KEY_BACKSPACE) && !_text_buffer.buffer.empty() && _text_buffer.cursor > 0 && input_window) {
        _text_buffer.delete_characters();
        bs_t = GetTime() + input_timeout;
    }
    if (IsKeyDown(KEY_RIGHT) && input_window) {
        _text_buffer.move_cursor_right();
        bs_t = GetTime() + input_timeout;
    }
    if (IsKeyDown(KEY_LEFT) && input_window) {
        _text_buffer.move_cursor_left();
        bs_t = GetTime() + input_timeout;
    }
    while ((k = GetKeyPressed())) {
        if (k == KEY_ENTER) {
            _text_buffer.push_character('\n');
        }
        if (k == KEY_TAB) {
            for (auto i = 0; i < 4; i++) {
                _text_buffer.push_character(' ');
            }
        }
        if (k == KEY_DOWN) {
            _text_buffer.move_cursor_down();
        }
        if (k == KEY_UP) {
            _text_buffer.move_cursor_up();
        }
        if (k == KEY_END) {
            _text_buffer.jump_cursor_to_end();
        }
    }
    _text_buffer.normalize_cursor_position();
    int c = 0;
    while ((c = GetCharPressed())) {
        _text_buffer.push_character(c);
        TraceLog(LOG_INFO, "%c", c);
        std::flush(std::cout);
    }
}

void draw_buffer(void)
{
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
    _text_buffer.buffer = "Welcome to Bootleg\nWelcome to Bootleg\nWelcome to Bootleg\nWelcome to Bootleg\n";
    DEFER(CloseWindow());
    while (!WindowShouldClose()) {
        update_buffer();
        BeginDrawing();
        ClearBackground(BLACK);
        draw_buffer();
        EndDrawing();
    }
}
