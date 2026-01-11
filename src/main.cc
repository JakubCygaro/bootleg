#include "defer.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <ostream>
#include <print>
#include <raylib.h>

#define CTRL true
#define META true
#define SHIFT true
#define NO false

constexpr const int FONT_SIZE = 24;

#define IsKeyPressedOrRepeat(KEY) (IsKeyPressed(KEY) || IsKeyPressedRepeat(KEY))
#define AnySpecialDown(SPECIAL_KEY) (IsKeyDown(KEY_LEFT_##SPECIAL_KEY) || IsKeyDown(KEY_RIGHT_##SPECIAL_KEY))

struct TextBuffer {
    using char_t = char;
    using line_t = std::basic_string<char8_t>;
    struct Cursor {
        long line {};
        long col {};
    };
    std::vector<line_t> lines = { {} };
    Cursor cursor = {};
    Font font {};
    int font_size = FONT_SIZE;
    int spacing = 10;
    void increase_font_size()
    {
        font_size = std::clamp(font_size + 1, 10, 60);
    }
    void decrease_font_size()
    {
        font_size = std::clamp(font_size - 1, 10, 60);
    }
    bool is_cursor_at_begining(void)
    {
        return (cursor.col == 0 && cursor.line == 0);
    }
    bool is_cursor_at_end(void)
    {
        return (cursor.col == lines.back().size() && cursor.line == lines.size() - 1);
    }
    line_t& current_line(void)
    {
        return lines[cursor.line];
    }
    const line_t& current_line(void) const
    {
        return lines[cursor.line];
    }
    std::optional<line_t::value_type> get_char_under_cursor(void) const
    {
        if (cursor.col == 0)
            return std::nullopt;
        return current_line()[cursor.col - 1];
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
    void move_cursor_word(long amount)
    {
        if (!amount)
            return;
        const auto amount_abs = std::abs(amount);
        const auto inc = (amount / std::abs(amount));
        auto end_check = [&]() {
            return (inc > 0 && is_cursor_at_end()) || (inc < 0 && is_cursor_at_begining());
        };
        auto prev = get_char_under_cursor();
        auto i = 0;
        while (std::abs(i) < amount_abs && !end_check()) {
            move_cursor_h(inc);
            auto c = get_char_under_cursor();
            if (c && prev) {
                if (std::isalnum(prev.value()) && (!std::isalnum(c.value()))) {
                    i += inc;
                }
            }
            prev = c;
        }
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
        if (cursor.col < 0 && cursor.line != 0) {
            cursor.line--;
            cursor.col = (long)current_line().size();
        } else if (cursor.col > (long)current_line().size() && cursor.line != lines.size() - 1) {
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
        if (lines.size() == 1)
            return;
        lines.erase(lines.begin() + line_num);
        clamp_cursor();
    }
    bool concat_backward(void)
    {
        if (cursor.line > 0) {
            cursor.line--;
            jump_cursor_to_end();
            current_line().append(lines[cursor.line + 1]);
            delete_line(cursor.line + 1);
            return true;
        }
        return false;
    }
    bool concat_forward(void)
    {
        if (cursor.line < lines.size() - 1) {
            current_line().append(lines[cursor.line + 1]);
            delete_line(cursor.line + 1);
            return true;
        }
        return false;
    }
    void delete_characters_back(unsigned long amount = 1)
    {
        auto to_delete = amount;
        while (true) {
            auto col = cursor.col;
            auto chars = cursor.col;
            if (chars < to_delete) {
                if (!concat_backward()) {
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
        while (true) {
            auto col = cursor.col;
            auto chars = current_line().size() - col;
            if (chars < to_delete) {
                if (!concat_forward()) {
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
    void insert_character(char_t c)
    {
        current_line().push_back('!');
        std::shift_right(current_line().begin() + cursor.col, current_line().end(), 1);
        current_line()[cursor.col++] = static_cast<char_t>(c);
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

static size_t encode_utf8(int utf, char* buf)
{
    printf("%016b\n", utf);
    if(utf > 0 && utf <= 0x007F){
        buf[0] = 0;
        buf[0] |= utf;
        buf[0] &= (0xFF >> 1);
        return 1;
    } else if (utf >= 0x0080 && utf <= 0x07FF){
        // byte 1
        buf[0] = 0;
        buf[0] = (utf >> 6);
        buf[0] |= (char)0b11000000;
        buf[0] &= (char)0b11011111;
        // byte 2
        buf[1] = utf;
        buf[1] |= (char)0b10000000;
        buf[1] &= (char)0b10111111;
        return 2;
    }
    return 1;
}

void update_buffer(void)
{
    if (IsKeyPressedOrRepeat(KEY_LEFT)) {
        if (AnySpecialDown(CONTROL)) {
            _text_buffer.move_cursor_word(-1);
        } else {
            _text_buffer.move_cursor_left();
        }
    }
    if (IsKeyPressedOrRepeat(KEY_H) && AnySpecialDown(CONTROL)) {
        _text_buffer.move_cursor_left();
    }
    if (IsKeyPressedOrRepeat(KEY_B) && AnySpecialDown(CONTROL)) {
        _text_buffer.move_cursor_word(-1);
    }
    if (IsKeyPressedOrRepeat(KEY_RIGHT)) {
        if (AnySpecialDown(CONTROL)) {
            _text_buffer.move_cursor_word(1);
        } else {
            _text_buffer.move_cursor_right();
        }
    }
    if (IsKeyPressedOrRepeat(KEY_L) && AnySpecialDown(CONTROL)) {
        _text_buffer.move_cursor_right();
    }
    if (IsKeyPressedOrRepeat(KEY_W) && AnySpecialDown(CONTROL)) {
        _text_buffer.move_cursor_word(1);
    }
    if (IsKeyPressedOrRepeat(KEY_UP)) {
        _text_buffer.move_cursor_up();
    }
    if (IsKeyPressedOrRepeat(KEY_K) && AnySpecialDown(CONTROL)) {
        _text_buffer.move_cursor_up();
    }
    if (IsKeyPressedOrRepeat(KEY_DOWN)) {
        _text_buffer.move_cursor_down();
    }
    if (IsKeyPressedOrRepeat(KEY_J) && AnySpecialDown(CONTROL)) {
        _text_buffer.move_cursor_down();
    }
    if (IsKeyPressedOrRepeat(KEY_END)) {
        _text_buffer.jump_cursor_to_end();
    }
    if (IsKeyPressedOrRepeat(KEY_HOME)) {
        _text_buffer.jump_cursor_to_start();
    }
    if (IsKeyPressedOrRepeat(KEY_BACKSPACE)) {
        _text_buffer.delete_characters_back();
    }
    if (IsKeyPressedOrRepeat(KEY_DELETE)) {
        _text_buffer.delete_characters_forward();
    }
    if (IsKeyPressedOrRepeat(KEY_ENTER)) {
        _text_buffer.insert_newline();
    }
    if (IsKeyPressedOrRepeat(KEY_O) && AnySpecialDown(CONTROL)) {
        _text_buffer.insert_newline();
    }
    if (IsKeyPressedOrRepeat(KEY_TAB)) {
        for (auto i = 0; i < 4; i++) {
            _text_buffer.insert_character(' ');
        }
    }
    if (IsKeyPressedOrRepeat(KEY_EQUAL) && AnySpecialDown(SHIFT) && AnySpecialDown(CONTROL)) {
        _text_buffer.increase_font_size();
    }
    if (IsKeyPressedOrRepeat(KEY_MINUS) && AnySpecialDown(CONTROL)) {
        _text_buffer.decrease_font_size();
    }
    _text_buffer.clamp_cursor();
    static char utfbuf[4] = { 0 };
    int c = 0;
    char* cptr = reinterpret_cast<char*>(&c);
    while ((c = GetCharPressed())) {
        TraceLog(LOG_INFO, "U+%04x", c);

        auto len = encode_utf8(c, utfbuf);
        for (auto i = 0; i < len; i++) {
            _text_buffer.insert_character(utfbuf[i]);
        }
    }
}

void draw_buffer(void)
{
    const Font font = _text_buffer.font;
    Vector2 pos = { 0, 0 };
    const float scale_factor = _text_buffer.font_size / (float)font.baseSize;
    const float line_advance = font.recs[GetGlyphIndex(font, ' ')].height * scale_factor;
    Rectangle last_c_r = { 0 };
    for (std::size_t linen = 0; linen < _text_buffer.get_line_count(); linen++) {
        auto& current_line = _text_buffer.lines[linen];
        for (size_t col = 0; col < current_line.size();) {
            // int c = current_line[col];
            int csz = 1;
            int c = GetCodepoint((char*)&current_line.data()[col], &csz);
            // if (csz > 1) {
            //     std::printf("ż%d %.*s\n",csz, csz, (char*)&current_line.data()[col]);
            //     std::flush(std::cout);
            // }
            int idx = GetGlyphIndex(font, c);
            float glyph_width = (font.glyphs[idx].advanceX == 0) ? font.recs[idx].width * scale_factor : font.glyphs[idx].advanceX * scale_factor;
            float glyph_height = font.recs[idx].height * scale_factor;

            auto r = GetGlyphAtlasRec(font, idx);
            if (linen == _text_buffer.cursor.line && col == _text_buffer.cursor.col) {
                DrawRectangleRec(Rectangle {
                                     .x = pos.x,
                                     .y = pos.y,
                                     .width = 2.,
                                     .height = line_advance },
                    WHITE);
            }
            DrawTextCodepoint(font, c, pos, _text_buffer.font_size, WHITE);
            pos.x += glyph_width + 2;
            col += csz;
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
static void add_codepoints_range(Font* font, const char* fontPath, int start, int stop)
{
    int rangeSize = stop - start + 1;
    int currentRangeSize = font->glyphCount;

    // TODO: Load glyphs from provided vector font (if available),
    // add them to existing font, regenerating font image and texture

    int updatedCodepointCount = currentRangeSize + rangeSize;
    int* updatedCodepoints = new int[updatedCodepointCount];
    DEFER(delete[] updatedCodepoints);

    // Get current codepoint list
    for (int i = 0; i < currentRangeSize; i++)
        updatedCodepoints[i] = font->glyphs[i].value;

    // Add new codepoints to list (provided range)
    for (int i = currentRangeSize; i < updatedCodepointCount; i++)
        updatedCodepoints[i] = start + (i - currentRangeSize);

    UnloadFont(*font);
    *font = LoadFontEx(fontPath, 32, updatedCodepoints, updatedCodepointCount);
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
    _text_buffer.lines = { { u8"int main(void){",
        u8"    printf(\"Hello, World!\");",
        u8"    return 0;",
        u8"}ąąą" } };
    InitWindow(800, 600, "bootleg");
    DEFER(CloseWindow());
    _text_buffer.font = argc == 2 ? (try_load_font(args[1])) : GetFontDefault();
    DEFER(
        if (_text_buffer.font.texture.id != GetFontDefault().texture.id)
            UnloadFont(_text_buffer.font););
    SetTargetFPS(60);
    while (!WindowShouldClose()) {
        update_buffer();
        BeginDrawing();
        ClearBackground(BLACK);
        draw_buffer();
        EndDrawing();
    }
}
