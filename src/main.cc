#include "defer.hpp"
#include "utf8.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <print>
#include <raylib.h>

constexpr const int FONT_SIZE = 24;
constexpr const int GLYPH_SPACING = 2;

#define IsKeyPressedOrRepeat(KEY) (IsKeyPressed(KEY) || IsKeyPressedRepeat(KEY))
#define AnySpecialDown(SPECIAL_KEY) (IsKeyDown(KEY_LEFT_##SPECIAL_KEY) || IsKeyDown(KEY_RIGHT_##SPECIAL_KEY))

struct TextBuffer {
    using char_t = char8_t;
    using line_t = std::basic_string<char_t>;
    struct Line {
        line_t contents {};
        std::optional<Rectangle> dims {};
    };
    struct Cursor {
        long line {};
        long col {};
        bool operator==(const Cursor& b) const
        {
            return (this->line == b.line) && (this->col == b.col);
        }
        bool operator!=(const Cursor& b) const
        {
            return !(b == *this);
        }
        bool operator<(const Cursor& b) const
        {
            return (this->line < b.line) || (this->line == b.line && this->col < b.col);
        }
        bool operator>(const Cursor& b) const
        {
            return b < *this;
        }
        bool operator<=(const Cursor& b) const
        {
            return (*this < b) || (*this == b);
        }
        bool operator>=(const Cursor& b) const
        {
            return (*this > b) || (*this == b);
        }
    };
    std::vector<Line> lines = { {} };
    Cursor cursor = {};
    Font font {};
    struct Selection {
        Cursor start {};
        Cursor end {};
        bool is_cursor_within(const Cursor& c) const noexcept{
            return (c > this->start && c <= this->end);
        }
    };

private:
    std::optional<Selection> selection {};

public:
    int font_size = FONT_SIZE;
    int spacing = 10;
    Color foreground_color = WHITE;
    Color background_color = BLACK;
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
        return (cursor.col == (long)lines.back().contents.size() && cursor.line == (long)lines.size() - 1);
    }
    line_t& current_line(void)
    {
        return lines[cursor.line].contents;
    }
    const line_t& current_line(void) const
    {
        return lines[cursor.line].contents;
    }
    const std::optional<Selection>& get_selection(void){
        return selection;
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
        if (current_line().empty() || cursor.col >= (long)current_line().size())
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
    void delete_selection(void){
        if(!selection) return;
        auto& start = selection->start;
        auto& end = selection->end;
        if(start.line == end.line){
            cursor.col = start.col;
            auto& start_line = lines[start.line];
            start_line.contents.erase(start_line.contents.begin() + start.col, start_line.contents.begin()+ end.col);
        } else {
            cursor = start;
            auto& start_line = lines[start.line].contents;
            // erase in start line
            if(start_line.empty()){
                move_cursor_up();
                jump_cursor_to_end();
                delete_line(start.line);
                start.line--;
                end.line--;
            } else {
                start_line.erase(start_line.begin() + start.col, start_line.end());
            }
            // erase in end line
            auto& end_line = lines[end.line].contents;
            if(end_line.empty()){
                delete_line(end.line);
            } else {
                end_line.erase(end_line.begin(), end_line.begin() + end.col);
            }
            auto line_diff = end.line - start.line - 1;
            delete_lines(start.line + 1, start.line + line_diff);
            concat_forward();
        }
        selection = std::nullopt;
    }
    long move_cursor_word(long amount, bool with_selection = false)
    {
        if (!amount)
            return 0;
        const auto amount_abs = std::abs(amount);
        const auto inc = (amount / std::abs(amount));
        const auto end_check = [&]() {
            return (inc > 0 && is_cursor_at_end()) || (inc < 0 && is_cursor_at_begining());
        };
        // bool is_under_punct = false;
        const auto under_check = [&](std::optional<char_t>& u) {
            // is_under_punct = false;
            if (!u.has_value())
                return true;
            auto uc = u.value();
            return (bool)std::isspace(uc) || ((bool)!std::isalnum(uc) && utf8::get_utf8_bytes_len(uc) != -1);
            // is_under_punct = std::ispunct(uc);
            // return (std::isspace(uc) || is_under_punct);
        };
        const auto after_check = [](std::optional<char_t>& a) {
            if (!a.has_value())
                return false;
            auto ac = a.value();
            return ((bool)std::isalnum(ac) || utf8::get_utf8_bytes_len(ac) != 1) && !std::isspace(ac);
            // return ((bool)std::isalnum(ac) || (bool)std::ispunct(ac) || utf8::get_utf8_bytes_len(ac) != 1);
        };
        const auto is_punct = [](std::optional<char_t>& a) {
            if (!a)
                return false;
            return (bool)std::ispunct(a.value());
        };
        auto moved = 0;
        auto i = 0;
        while (i < amount_abs && !end_check()) {
            moved += move_cursor_h(inc, with_selection);
            auto u = get_char_under_cursor();
            auto a = get_char_after_cursor();
            if (is_punct(a) || (under_check(u) && after_check(a)))
                i++;
        }
        return moved;
    }
    long move_cursor_left(long amount = 1, bool with_selection = false)
    {
        return move_cursor_h(-amount, with_selection);
    }
    long move_cursor_right(long amount = 1, bool with_selection = false)
    {
        return move_cursor_h(amount, with_selection);
    }
    // returns how many positions the cursor moved (across lines, and counted in bytes)
    long move_cursor_h(long amount, bool with_selection = false)
    {
        if(with_selection && !selection)
            start_selection();
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
                // return moved;
                break;
            } else if (cursor.col > (long)current_line().size() && cursor.line == (long)lines.size() - 1) {
                cursor.col = (long)current_line().size();
                // return moved;
                break;
            } else {
                moved++;
            }
            // clamp_cursor();
        }
        if(with_selection) update_selection();
        return moved;
    }
    void update_selection(void){

        if(cursor < selection->start){
            // selection->end = selection->start;
            // selection->start = { cursor.line, cursor.col - 1 };
            selection->start = cursor;
        }
        else {
            selection->end = { cursor.line, cursor.col};
        }
    }
    long count_chars_to_cursor_in_line(void){
        const auto& line = current_line();
        long chars = 0;
        for(long i = 0; i < (long)line.size();){
            if(i == cursor.col) break;
            i += utf8::get_utf8_bytes_len(line[i]);
            chars++;
        }
        return chars;
    }
    long move_cursor_v(long amount, bool with_selection = false)
    {
        if(with_selection && !selection)
            start_selection();
        if (cursor.line + amount < 0) {
            amount = -cursor.line;
        } else if (cursor.line + amount > (long)lines.size() - 1) {
            amount = (long)lines.size() - 1 - cursor.line;
        }
        auto chars = count_chars_to_cursor_in_line();
        cursor.line += amount;
        jump_cursor_to_start();
        if(chars == 0)
            jump_cursor_to_start();
        else if((long)current_line().size() < chars )
            jump_cursor_to_end();
        else
            move_cursor_right(chars);
        // auto current = get_char_under_cursor();
        // if (current) {
        //     auto crlen = utf8::get_utf8_bytes_len(current.value());
        //     if (crlen > 1) {
        //         cursor.col += crlen - 1;
        //     } else if (crlen == -1) {
        //         auto next = get_char_after_cursor();
        //         while (next.has_value() && utf8::get_utf8_bytes_len(next.value()) == -1) {
        //             next = get_char_after_cursor();
        //             cursor.col++;
        //         }
        //     }
        // }
        // cursor.col = std::clamp(cursor.col, (long)0, (long)current_line().size());
        if(with_selection) update_selection();
        return amount;
    }
    void move_cursor_down(long amount = 1, bool with_selection = false)
    {
        move_cursor_v(amount, with_selection);
    }
    void move_cursor_up(long amount = 1, bool with_selection = false)
    {
        move_cursor_v(-amount, with_selection);
    }
    void delete_line(size_t line_num)
    {
        if (lines.size() == 1){
            lines[0].contents.erase();
            return;
        }
        lines.erase(lines.begin() + line_num);
        clamp_cursor();
    }
    void delete_lines(size_t start, size_t end)
    {
        if (lines.size() == 1){
            lines[0].contents.erase();
            return;
        }
        lines.erase(lines.begin() + start, lines.begin() + end + 1);
        clamp_cursor();
    }
    bool concat_backward(void)
    {
        if (cursor.line > 0) {
            cursor.line--;
            jump_cursor_to_end();
            current_line().append(lines[cursor.line + 1].contents);
            delete_line(cursor.line + 1);
            return true;
        }
        return false;
    }
    bool concat_forward(void)
    {
        if (cursor.line < (long)lines.size() - 1) {
            current_line().append(lines[cursor.line + 1].contents);
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
    void delete_words_back(unsigned long amount = 1)
    {
        auto end = cursor.line;
        auto moved = move_cursor_word(-amount);
        auto start = cursor.line;
        auto line_diff = end - start;
        while (line_diff-- > 0) {
            concat_forward();
        }
        current_line().erase(current_line().begin() + cursor.col, current_line().begin() + cursor.col + moved);
    }
    void delete_words_forward(unsigned long amount = 1)
    {
        auto start = cursor.line;
        auto start_col = cursor.col;
        move_cursor_word(amount);
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
    void jump_cursor_to_end(bool with_selection = false)
    {
        cursor.col = current_line().size();
        if(with_selection) update_selection();
    }
    void jump_cursor_to_start(bool with_selection = false)
    {
        cursor.col = 0;
        if(with_selection) update_selection();
    }
    void insert_newline(void)
    {
        lines.resize(lines.size() + 1);
        std::shift_right(lines.begin() + cursor.line + 1, lines.end(), 1);
        lines.insert(lines.begin() + cursor.line + 1, {});
        auto& next_line = lines[cursor.line + 1].contents;
        if (cursor.col < (long)current_line().size()) {
            next_line.resize(current_line().size() - cursor.col);
            std::copy(current_line().begin() + cursor.col, current_line().end(), next_line.begin());
            current_line().erase(current_line().begin() + cursor.col, current_line().end());
        }
        cursor.line++;
        cursor.col = 0;
    }
    void start_selection(void)
    {
        selection = { cursor, cursor };
    }
    void clear_selection(void)
    {
        selection = std::nullopt;
    }
};

static TextBuffer _text_buffer = {};

TextBuffer::Cursor detect_point_over_buffer(const Vector2 point)
{
    const Font font = _text_buffer.font;
    const float scale_factor = _text_buffer.font_size / (float)font.baseSize;
    const float line_advance = font.recs[GetGlyphIndex(font, ' ')].height * scale_factor;
    long linen = point.y == 0 ? 0 : (long)(point.y / line_advance) % _text_buffer.get_line_count();
    auto& line = _text_buffer.lines[linen];
    if (line.contents.size() == 0)
        return TextBuffer::Cursor { .line = linen, .col = 0 };
    float advance = 0;
    for (long col = 0; col < (long)line.contents.size();) {
        int csz = 1;
        int c = GetCodepoint((char*)&line.contents.data()[col], &csz);
        int idx = GetGlyphIndex(font, c);
        float glyph_width = (font.glyphs[idx].advanceX == 0) ? font.recs[idx].width * scale_factor : font.glyphs[idx].advanceX * scale_factor;
        // auto r = GetGlyphAtlasRec(font, idx);
        if (point.x >= advance && point.x <= advance + glyph_width + GLYPH_SPACING)
            return TextBuffer::Cursor { .line = linen, .col = col };
        advance += glyph_width + GLYPH_SPACING;
        col += csz;
    }
    return TextBuffer::Cursor { .line = linen, .col = (long)line.contents.size() };
}

void update_buffer_mouse(void)
{
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        _text_buffer.cursor = detect_point_over_buffer(GetMousePosition());
    }
}

void update_buffer(void)
{
    update_buffer_mouse();
    const bool shift_down = AnySpecialDown(SHIFT);
    const auto start_pos = _text_buffer.cursor;
    if (IsKeyPressedOrRepeat(KEY_LEFT)) {
        if (AnySpecialDown(CONTROL)) {
            _text_buffer.move_cursor_word(-1, shift_down);
        } else {
            _text_buffer.move_cursor_left(1, shift_down);
        }
    }
    if (IsKeyPressedOrRepeat(KEY_H) && AnySpecialDown(CONTROL)) {
        _text_buffer.move_cursor_left(1, shift_down);
    }
    if (IsKeyPressedOrRepeat(KEY_B) && AnySpecialDown(CONTROL)) {
        _text_buffer.move_cursor_word(-1, shift_down);
    }
    if (IsKeyPressedOrRepeat(KEY_RIGHT)) {
        if (AnySpecialDown(CONTROL)) {
            _text_buffer.move_cursor_word(1, shift_down);
        } else {
            _text_buffer.move_cursor_right(1, shift_down);
        }
    }
    if (IsKeyPressedOrRepeat(KEY_L) && AnySpecialDown(CONTROL)) {
        _text_buffer.move_cursor_right(1, shift_down);
    }
    if (IsKeyPressedOrRepeat(KEY_W) && AnySpecialDown(CONTROL)) {
        _text_buffer.move_cursor_word(1, shift_down);
    }
    if (IsKeyPressedOrRepeat(KEY_UP)) {
        _text_buffer.move_cursor_up(1, shift_down);
    }
    if (IsKeyPressedOrRepeat(KEY_K) && AnySpecialDown(CONTROL)) {
        _text_buffer.move_cursor_up(1, shift_down);
    }
    if (IsKeyPressedOrRepeat(KEY_DOWN)) {
        _text_buffer.move_cursor_down(1, shift_down);
    }
    if (IsKeyPressedOrRepeat(KEY_J) && AnySpecialDown(CONTROL)) {
        _text_buffer.move_cursor_down(1, shift_down);
    }
    if (IsKeyPressedOrRepeat(KEY_END)) {
        _text_buffer.jump_cursor_to_end(shift_down);
    }
    if (IsKeyPressedOrRepeat(KEY_HOME)) {
        _text_buffer.jump_cursor_to_start(shift_down);
    }
    if (IsKeyPressedOrRepeat(KEY_BACKSPACE)) {
        if (AnySpecialDown(CONTROL))
            _text_buffer.delete_words_back();
        else if(_text_buffer.get_selection().has_value())
            _text_buffer.delete_selection();
        else
            _text_buffer.delete_characters_back();
    }
    if (IsKeyPressedOrRepeat(KEY_DELETE)) {
        if (AnySpecialDown(CONTROL))
            _text_buffer.delete_words_forward();
        else
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
    // _text_buffer.clamp_cursor();
    static unsigned char utfbuf[4] = { 0 };
    int c = 0;
    while ((c = GetCharPressed())) {
        auto len = utf8::encode_utf8(c, utfbuf);
        for (size_t i = 0; i < len; i++) {
            _text_buffer.insert_character(utfbuf[i]);
        }
    }
    if(start_pos != _text_buffer.cursor && !shift_down)
        _text_buffer.clear_selection();
}

void draw_buffer(void)
{
    // for selection checking
    TextBuffer::Cursor cursor = {};
    const Font font = _text_buffer.font;
    Vector2 pos = { 0, 0 };
    const float scale_factor = _text_buffer.font_size / (float)font.baseSize;
    const float line_advance = font.recs[GetGlyphIndex(font, ' ')].height * scale_factor;
    Rectangle last_c_r = {};
    for (std::size_t linen = 0; linen < _text_buffer.get_line_count(); linen++) {
        auto& current_line = _text_buffer.lines[linen];
        for (size_t col = 0; col < current_line.contents.size();) {
            int csz = 1;
            int c = GetCodepoint((char*)&current_line.contents.data()[col], &csz);
            int idx = GetGlyphIndex(font, c);
            const float glyph_width = (font.glyphs[idx].advanceX == 0) ? font.recs[idx].width * scale_factor : font.glyphs[idx].advanceX * scale_factor;
            // float glyph_height = font.recs[idx].height * scale_factor;

            auto r = GetGlyphAtlasRec(font, idx);
            if ((long)linen == _text_buffer.cursor.line && (long)col == _text_buffer.cursor.col) {
                DrawRectangleRec(Rectangle {
                                     .x = pos.x,
                                     .y = pos.y,
                                     .width = GLYPH_SPACING,
                                     .height = line_advance },
                    WHITE);
            }
            cursor.col = col + 1;
            cursor.line = linen;
            if(_text_buffer.get_selection().has_value() && _text_buffer.get_selection()->is_cursor_within(cursor)){
                DrawRectangleRec(Rectangle {
                        .x = pos.x,
                        .y = pos.y,
                        .width = glyph_width + GLYPH_SPACING,
                        .height = line_advance },
                        _text_buffer.foreground_color);
                DrawTextCodepoint(font, c, pos, _text_buffer.font_size, _text_buffer.background_color);
            } else {
                DrawTextCodepoint(font, c, pos, _text_buffer.font_size, _text_buffer.foreground_color);
            }
            pos.x += glyph_width + GLYPH_SPACING;
            col += csz;
            last_c_r = r;
        }
        if (_text_buffer.cursor.line == (long)linen && _text_buffer.cursor.col == (long)current_line.contents.size()) {
            DrawRectangleRec(Rectangle {
                                 .x = pos.x,
                                 .y = pos.y,
                                 .width = GLYPH_SPACING,
                                 .height = line_advance },
                _text_buffer.foreground_color);
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
        u8"Welcome to Bootleg! ąąą ęęę śðæśðæ",
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
