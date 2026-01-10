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
    using line_t = std::string;
    struct Cursor {
        long line {};
        long col {};
    };
    std::vector<line_t> lines = {{}};
    Cursor cursor = {};
    line_t& current_line(void)
    {
        return lines[cursor.line];
    }
    size_t get_line_count(void) const
    {
        return lines.size();
    }
    void clamp_cursor(void)
    {
        cursor.line = std::clamp(cursor.line, (long)0, (long)lines.size() - 1);
        cursor.col = std::clamp(cursor.col, (long)0, (long)current_line().size());
    }
    void move_cursor_left(long amount = 1)
    {
        move_cursor_h(-amount);
    }
    void move_cursor_right(long amount = 1)
    {
        move_cursor_h(amount);
    }
    void move_cursor_h(long amount)
    {
        cursor.col += amount;
        if(cursor.col < 0 && cursor.line != 0) {
            cursor.line--;
            cursor.col = (long)current_line().size();
        } else if(cursor.col > (long)current_line().size() && cursor.line != lines.size() - 1) {
            cursor.line++;
            cursor.col = 0;
        }
        clamp_cursor();
    }
    void move_cursor_v(long amount)
    {
        cursor.line += amount;
        clamp_cursor();
    }
    void move_cursor_down(void)
    {
        move_cursor_v(1);
    }
    void move_cursor_up(void)
    {
        move_cursor_v(-1);
    }
    void delete_line(size_t line_num)
    {
        if(lines.size() == 1)
            return;
        lines.erase(lines.begin() + line_num);
        clamp_cursor();
    }
    bool concat_backward(void) {
        if(cursor.line > 0){
            cursor.line--;
            jump_cursor_to_end();
            current_line().append(lines[cursor.line + 1]);
            delete_line(cursor.line + 1);
            return true;
        }
        return false;
    }
    bool concat_forward(void) {
        if(cursor.line < lines.size() - 1){
            current_line().append(lines[cursor.line + 1]);
            delete_line(cursor.line + 1);
            return true;
        }
        return false;
    }
    void delete_characters_back(unsigned long amount = 1)
    {
        auto to_delete = amount;
        while(true){
            auto col = cursor.col;
            auto chars = cursor.col;
            if (chars < to_delete) {
                if(!concat_backward()) {
                    current_line().erase(current_line().begin(), current_line().begin() + cursor.col);
                    return;
                }
                to_delete--;
            } else {
                current_line().erase(cursor.col - to_delete, to_delete);
                move_cursor_left(to_delete);
                break;
            }
        }
    }
    void delete_characters_forward(unsigned long amount = 1)
    {
        auto to_delete = amount;
        while(true){
            auto col = cursor.col;
            auto chars = current_line().size() - col;
            if (chars < to_delete) {
                if(!concat_forward()) {
                    current_line().erase(current_line().begin() + cursor.col, current_line().end());
                    return;
                }
                to_delete--;
            } else {
                current_line().erase(cursor.col, to_delete);
                break;
            }
        }
    }
    void push_character(char c)
    {
        current_line().push_back('!');
        std::shift_right(current_line().begin() + cursor.col, current_line().end(), 1);
        current_line()[cursor.col++] = static_cast<char>(c);
    }
    void jump_cursor_to_end(void)
    {
        cursor.col = current_line().size();
    }
    void jump_cursor_to_start(void)
    {
        cursor.col = 0;
    }
    void insert_newline(void)
    {
        lines.resize(lines.size() + 1);
        std::shift_right(lines.begin() + cursor.line + 1, lines.end(), 1);
        lines.insert(lines.begin() + cursor.line + 1, {});
        auto& next_line = lines[cursor.line + 1];
        if (cursor.col < current_line().size()) {
            next_line.resize(current_line().size() - cursor.col);
            std::copy(current_line().begin() + cursor.col, current_line().end(), next_line.begin());
            current_line().erase(current_line().begin() + cursor.col, current_line().end());
        }
        cursor.line++;
        cursor.col = 0;
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
        _text_buffer.delete_characters_back();
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
    if (IsKeyDown(KEY_BACKSPACE) && input_window) {
        _text_buffer.delete_characters_back();
        bs_t = GetTime() + input_timeout;
    }
    if (IsKeyDown(KEY_DELETE) && input_window) {
        _text_buffer.delete_characters_forward();
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
            _text_buffer.insert_newline();
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
    _text_buffer.clamp_cursor();
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
    const float line_advance = font.recs[GetGlyphIndex(font, ' ')].height * scale_factor;
    Rectangle last_c_r = { 0 };
    for (std::size_t linen = 0; linen < _text_buffer.get_line_count(); linen++) {
        auto& current_line = _text_buffer.lines[linen];
        for (size_t col = 0; col < current_line.size(); col++) {
            char c = current_line[col];
            int idx = GetGlyphIndex(font, c);
            float glyph_width = (font.glyphs[idx].advanceX == 0) ? font.recs[idx].width * scale_factor : font.glyphs[idx].advanceX * scale_factor;
            float glyph_height = font.recs[idx].height * scale_factor;

            auto r = GetGlyphAtlasRec(font, idx);
            if (linen == _text_buffer.cursor.line && col == _text_buffer.cursor.col) {
                DrawRectangleRec(Rectangle {
                                     .x = pos.x,
                                     .y = pos.y,
                                     .width = 2.,
                                     .height = glyph_height },
                    WHITE);
            }
            DrawTextCodepoint(font, c, pos, 24, WHITE);
            pos.x += glyph_width + 2;

            last_c_r = r;
        }
        if (_text_buffer.cursor.line == linen && _text_buffer.cursor.col == current_line.size()) {
            DrawRectangleRec(Rectangle {
                                 .x = pos.x,
                                 .y = pos.y,
                                 .width = 2.,
                                 .height = line_advance },
                WHITE);
        }
        pos.x = 0;
        pos.y += line_advance;
    }
}

int main(void)
{
    InitWindow(800, 600, "bootleg");
    SetTargetFPS(60);
    _text_buffer.lines = {{
        "int main(void){",
        "    printf(\"Hello, World!\");",
        "    return 0;",
        "}"
    }};
    DEFER(CloseWindow());
    while (!WindowShouldClose()) {
        update_buffer();
        BeginDrawing();
        ClearBackground(BLACK);
        draw_buffer();
        EndDrawing();
    }
}
