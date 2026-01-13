#include "defer.hpp"
#include "utf8.hpp"
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
    using char_t = char8_t;
    using line_t = std::basic_string<char_t>;
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
        return (cursor.col == (long)lines.back().size() && cursor.line == (long)lines.size() - 1);
    }
    line_t& current_line(void)
    {
        return lines[cursor.line];
    }
    const line_t& current_line(void) const
    {
        return lines[cursor.line];
    }
    // if | is the cursor then x is the character
    //
    // x|a
    std::optional<line_t::value_type> get_char_under_cursor(void) const
    {
        if (cursor.col <= 0 || cursor.col > (long)current_line().size())
            return std::nullopt;
        return current_line()[cursor.col - 1];
    }
    // if | is the cursor then x is the character
    //
    // a|x
    std::optional<line_t::value_type> get_char_after_cursor(void) const
    {
        if (current_line().empty() || cursor.col == (long)current_line().size())
            return std::nullopt;
        return current_line()[cursor.col];
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
        // TODO: fix this
        if (!amount)
            return;
        const auto amount_abs = std::abs(amount);
        const auto inc = (amount / std::abs(amount));
        const auto end_check = [&]() {
            return (inc > 0 && is_cursor_at_end()) || (inc < 0 && is_cursor_at_begining());
        };
        const auto under_check = [](std::optional<char_t> u) {
            if(!u)
                return true;
            auto uc = u.value();
            return (bool)std::isspace(uc);
        };
        const auto after_check = [](std::optional<char_t> a) {
            if(!a)
                return false;
            auto ac = a.value();
            return (bool)std::isalnum(ac) || utf8::get_utf8_bytes_len(ac) != 1;
        };
        auto i = 0;
        while (i < amount_abs && !end_check()) {
            auto moved = move_cursor_h(inc);
            if (moved == 0)
                break;
            auto u = get_char_under_cursor();
            auto a = get_char_after_cursor();
            if(under_check(u) && after_check(a))
                i++;
        }
    }
    long move_cursor_left(long amount = 1)
    {
        return move_cursor_h(-amount);
    }
    long move_cursor_right(long amount = 1)
    {
        return move_cursor_h(amount);
    }
    // returns how many positions the cursor moved (across lines, and counted in bytes)
    long move_cursor_h(long amount)
    {
        const auto amount_abs = std::abs(amount);
        const auto inc = (amount / std::abs(amount));
        auto moved = 0;
        for (auto i = 0; i < amount_abs; i++) {
            auto prev = get_char_under_cursor();
            cursor.col += inc;
            auto current = get_char_under_cursor();
            if (current.has_value()) {
                char cr = current.value();
                auto crlen = utf8::get_utf8_bytes_len(cr);
                // moving forward
                if (inc > 0 && crlen > 1) {
                    cursor.col += crlen - 1;
                    moved++;
                }
                // moving backwards
                else if (inc < 0 && prev.has_value()) {
                    char pr = prev.value();
                    auto prlen = utf8::get_utf8_bytes_len(pr);
                    while (prlen == -1) {
                        crlen = utf8::get_utf8_bytes_len(get_char_under_cursor().value_or(' '));
                        // move back until you encounter a normal ascii character or a root of a unicode character
                        cursor.col += inc;
                        moved++;
                        if (crlen != -1)
                            break;
                    }
                }
            }
            if (cursor.col < 0 && cursor.line != 0) {
                cursor.line--;
                cursor.col = (long)current_line().size();
            } else if (cursor.col > (long)current_line().size() && cursor.line != (long)lines.size() - 1) {
                cursor.line++;
                cursor.col = 0;
            } else if (cursor.col < 0 && cursor.line == 0) {
                cursor.col = 0;
                return moved;
            } else if (cursor.col > (long)current_line().size() && cursor.line == (long)lines.size() - 1) {
                cursor.col = (long)current_line().size();
                return moved;
            } else {
                moved++;
            }
            // clamp_cursor();
        }
        return moved;
    }
    long move_cursor_v(long amount)
    {
        if (cursor.line + amount < 0) {
            amount = -cursor.line;
        } else if (cursor.line + amount > (long)lines.size() - 1) {
            amount = (long)lines.size() - 1 - cursor.line;
        }
        cursor.line += amount;
        auto current = get_char_under_cursor();
        if (current) {
            auto crlen = utf8::get_utf8_bytes_len(current.value());
            if (crlen > 1) {
                cursor.col += crlen - 1;
            } else if (crlen == -1) {
                auto next = get_char_after_cursor();
                while (next.has_value() && utf8::get_utf8_bytes_len(next.value()) == -1) {
                    next = get_char_after_cursor();
                    cursor.col++;
                }
            }
        }
        cursor.col = std::clamp(cursor.col, (long)0, (long)current_line().size());
        return amount;
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
        if (cursor.line < (long)lines.size() - 1) {
            current_line().append(lines[cursor.line + 1]);
            delete_line(cursor.line + 1);
            return true;
        }
        return false;
    }
    void delete_characters_back(unsigned long amount = 1)
    {
        auto end = cursor.line;
        auto moved = move_cursor_left(amount);
        auto start = cursor.line;
        auto line_diff = end - start;
        while (line_diff-- > 0) {
            concat_forward();
        }
        current_line().erase(current_line().begin() + cursor.col, current_line().begin() + cursor.col + moved);
    }
    void delete_characters_forward(unsigned long amount = 1)
    {
        auto start = cursor.line;
        auto start_col = cursor.col;
        move_cursor_right(amount);
        auto end = cursor.line;
        auto line_diff = end - start;
        while (line_diff-- > 0) {
            concat_backward();
        }
        current_line().erase(current_line().begin() + start_col, current_line().begin() + cursor.col);
        cursor.col = start_col;
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
        if (cursor.col < (long)current_line().size()) {
            next_line.resize(current_line().size() - cursor.col);
            std::copy(current_line().begin() + cursor.col, current_line().end(), next_line.begin());
            current_line().erase(current_line().begin() + cursor.col, current_line().end());
        }
        cursor.line++;
        cursor.col = 0;
    }
};

static TextBuffer _text_buffer = {};

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
    static unsigned char utfbuf[4] = { 0 };
    int c = 0;
    while ((c = GetCharPressed())) {
        auto len = utf8::encode_utf8(c, utfbuf);
        for (size_t i = 0; i < len; i++) {
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
    Rectangle last_c_r = {};
    for (std::size_t linen = 0; linen < _text_buffer.get_line_count(); linen++) {
        auto& current_line = _text_buffer.lines[linen];
        for (size_t col = 0; col < current_line.size();) {
            int csz = 1;
            int c = GetCodepoint((char*)&current_line.data()[col], &csz);
            int idx = GetGlyphIndex(font, c);
            float glyph_width = (font.glyphs[idx].advanceX == 0) ? font.recs[idx].width * scale_factor : font.glyphs[idx].advanceX * scale_factor;
            // float glyph_height = font.recs[idx].height * scale_factor;

            auto r = GetGlyphAtlasRec(font, idx);
            if ((long)linen == _text_buffer.cursor.line && (long)col == _text_buffer.cursor.col) {
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
        if (_text_buffer.cursor.line == (long)linen && _text_buffer.cursor.col == (long)current_line.size()) {
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
    _text_buffer.lines = { {
        u8"Welcome to Bootleg!",
    } };
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
