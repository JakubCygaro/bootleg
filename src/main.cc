#include "defer.hpp"
#include "utf8.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <optional>
#include <print>
#include <raylib.h>
#include <string>

#define IsKeyPressedOrRepeat(KEY) (IsKeyPressed(KEY) || IsKeyPressedRepeat(KEY))
#define AnySpecialDown(SPECIAL_KEY) (IsKeyDown(KEY_LEFT_##SPECIAL_KEY) || IsKeyDown(KEY_RIGHT_##SPECIAL_KEY))

struct TextBuffer {
    using char_t = char;
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
    struct Selection {
        Cursor start {};
        Cursor end {};
        bool is_cursor_within(const Cursor& c) const noexcept
        {
            if (this->start <= this->end)
                return (c > this->start && c <= this->end);
            else
                return (c > this->end && c <= this->start);
        }
    };

private:
    std::vector<Line> m_lines = { {} };
    std::optional<Selection> m_selection {};
    Cursor m_cursor = {};
    Font m_font {};
    Rectangle m_bounds = {};
    int m_font_size = 24;
    int m_spacing = 10;
    int m_glyph_spacing = 2;
    float m_scroll_v = 0.0;
    float m_v_scroll_bar_width {};

    float f_scale_factor;
    float f_line_advance;
    float f_total_height;

public:
    Color foreground_color = WHITE;
    Color background_color = BLACK;

    TextBuffer(Font f, Rectangle bounds)
        : m_font { f }
        , m_bounds { bounds }
    {
        update_font_measurements();
        m_v_scroll_bar_width = (m_bounds.width * 0.02f);
    }

    const Font& get_font() const
    {
        return m_font;
    }
    void set_font(Font font)
    {
        m_font = font;
        update_font_measurements();
    }
    int get_font_size() const
    {
        return m_font_size;
    }
    void set_font_size(int sz)
    {
        m_font_size = sz;
        update_font_measurements();
    }
    int get_spacing() const
    {
        return m_spacing;
    }
    void set_spacing(int s)
    {
        m_spacing = s;
        update_font_measurements();
    }
    void increase_font_size()
    {
        m_font_size = std::clamp(m_font_size + 1, 10, 60);
        update_font_measurements();
    }
    void decrease_font_size()
    {
        m_font_size = std::clamp(m_font_size - 1, 10, 60);
        update_font_measurements();
    }
    bool is_cursor_at_begining(void)
    {
        return (m_cursor.col == 0 && m_cursor.line == 0);
    }
    bool is_cursor_at_end(void)
    {
        return (m_cursor.col == (long)m_lines.back().contents.size() && m_cursor.line == (long)m_lines.size() - 1);
    }
    line_t& current_line(void)
    {
        return m_lines[m_cursor.line].contents;
    }
    const line_t& current_line(void) const
    {
        return m_lines[m_cursor.line].contents;
    }
    const std::optional<Selection>& get_selection(void)
    {
        return m_selection;
    }
    // if | is the cursor then x is the character
    //
    // x|a
    std::optional<line_t::value_type> get_char_under_cursor(void) const
    {
        if (m_cursor.col <= 0 || m_cursor.col > (long)current_line().size())
            return std::nullopt;
        return current_line()[m_cursor.col - 1];
    }
    // if | is the cursor then x is the character
    //
    // a|x
    std::optional<line_t::value_type> get_char_after_cursor(void) const
    {
        if (current_line().empty() || m_cursor.col >= (long)current_line().size())
            return std::nullopt;
        return current_line()[m_cursor.col];
    }
    size_t get_line_count(void) const
    {
        return m_lines.size();
    }
    void delete_selection(void)
    {
        if (!m_selection)
            return;
        auto& start = m_selection->start;
        auto& end = m_selection->end;
        if (start > end)
            std::swap(start, end);
        if (start.line == end.line) {
            m_cursor.col = start.col;
            auto& start_line = m_lines[start.line];
            start_line.contents.erase(start_line.contents.begin() + start.col, start_line.contents.begin() + end.col);
        } else {
            m_cursor = start;
            auto& start_line = m_lines[start.line].contents;
            // erase in start line
            if (start_line.empty()) {
                move_cursor_up();
                jump_cursor_to_end();
                delete_line(start.line);
                start.line--;
                end.line--;
            } else {
                start_line.erase(start_line.begin() + start.col, start_line.end());
            }
            // erase in end line
            auto& end_line = m_lines[end.line].contents;
            if (end_line.empty()) {
                delete_line(end.line);
            } else {
                end_line.erase(end_line.begin(), end_line.begin() + end.col);
            }
            auto line_diff = end.line - start.line - 1;
            delete_lines(start.line + 1, start.line + line_diff);
            concat_forward();
        }
        m_selection = std::nullopt;
    }
    line_t copy_selection(void)
    {
        if (!m_selection)
            return "";
        line_t out = {};
        auto& start = m_selection->start;
        auto& end = m_selection->end;
        if (start > end)
            std::swap(start, end);
        if (start.line == end.line) {
            m_cursor.col = start.col;
            auto& start_line = m_lines[start.line];
            out.insert(out.begin(), start_line.contents.begin() + start.col, start_line.contents.begin() + end.col);
        } else {
            m_cursor = start;
            auto& start_line = m_lines[start.line].contents;
            // copy in start line
            if (start_line.empty()) {
                out.push_back('\n');
            } else {
                out.append(start_line.begin() + start.col, start_line.end());
                out.push_back('\n');
            }
            // copy in between
            auto line_diff = end.line - start.line - 1;
            for (size_t i = start.line + 1; i <= (size_t)start.line + line_diff; i++) {
                auto& current_line = m_lines[i].contents;
                if (current_line.empty()) {
                    out.push_back('\n');
                } else {
                    out.append(current_line);
                    out.push_back('\n');
                }
            }
            // copy in end line
            auto& end_line = m_lines[end.line].contents;
            if (!end_line.empty()) {
                out.append(end_line.begin(), end_line.begin() + end.col);
            }
        }
        return out;
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
        if (with_selection && !m_selection)
            start_selection();
        const auto amount_abs = std::abs(amount);
        const auto inc = (amount / std::abs(amount));
        auto moved = 0;
        for (auto i = 0; i < amount_abs; i++) {
            auto prev = get_char_under_cursor();
            m_cursor.col += inc;
            auto current = get_char_under_cursor();
            if (current.has_value()) {
                char cr = current.value();
                auto crlen = utf8::get_utf8_bytes_len(cr);
                // moving forward
                if (inc > 0 && crlen > 1) {
                    m_cursor.col += crlen - 1;
                    moved++;
                }
                // moving backwards
                else if (inc < 0 && prev.has_value()) {
                    char pr = prev.value();
                    auto prlen = utf8::get_utf8_bytes_len(pr);
                    while (prlen == -1) {
                        crlen = utf8::get_utf8_bytes_len(get_char_under_cursor().value_or(' '));
                        // move back until you encounter a normal ascii character or a root of a unicode character
                        m_cursor.col += inc;
                        moved++;
                        if (crlen != -1)
                            break;
                    }
                }
            }
            if (m_cursor.col < 0 && m_cursor.line != 0) {
                m_cursor.line--;
                m_cursor.col = (long)current_line().size();
            } else if (m_cursor.col > (long)current_line().size() && m_cursor.line != (long)m_lines.size() - 1) {
                m_cursor.line++;
                m_cursor.col = 0;
            } else if (m_cursor.col < 0 && m_cursor.line == 0) {
                m_cursor.col = 0;
                // return moved;
                break;
            } else if (m_cursor.col > (long)current_line().size() && m_cursor.line == (long)m_lines.size() - 1) {
                m_cursor.col = (long)current_line().size();
                // return moved;
                break;
            } else {
                moved++;
            }
            // clamp_cursor();
        }
        if (with_selection)
            update_selection();
        update_viewport_to_cursor();
        return moved;
    }
    long count_chars_to_cursor_in_line(void)
    {
        const auto& line = current_line();
        long chars = 0;
        for (long i = 0; i < (long)line.size();) {
            if (i == m_cursor.col)
                break;
            i += utf8::get_utf8_bytes_len(line[i]);
            chars++;
        }
        return chars;
    }
    long move_cursor_v(long amount, bool with_selection = false)
    {
        if (with_selection && !m_selection)
            start_selection();
        if (m_cursor.line + amount < 0) {
            amount = -m_cursor.line;
        } else if (m_cursor.line + amount > (long)m_lines.size() - 1) {
            amount = (long)m_lines.size() - 1 - m_cursor.line;
        }
        auto chars = count_chars_to_cursor_in_line();
        m_cursor.line += amount;
        jump_cursor_to_start();
        if (chars == 0)
            jump_cursor_to_start();
        else if ((long)current_line().size() < chars)
            jump_cursor_to_end();
        else
            move_cursor_right(chars);
        if (with_selection)
            update_selection();
        update_viewport_to_cursor();
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
        if (m_lines.size() == 1) {
            m_lines[0].contents.erase();
            return;
        }
        m_lines.erase(m_lines.begin() + line_num);
        clamp_cursor();
        update_total_height();
        update_viewport_to_cursor();
    }
    void delete_lines(size_t start, size_t end)
    {
        if (m_lines.size() == 1) {
            m_lines[0].contents.erase();
            return;
        }
        m_lines.erase(m_lines.begin() + start, m_lines.begin() + end + 1);
        clamp_cursor();
        update_total_height();
        update_viewport_to_cursor();
    }
    bool concat_backward(void)
    {
        if (m_cursor.line > 0) {
            m_cursor.line--;
            jump_cursor_to_end();
            current_line().append(m_lines[m_cursor.line + 1].contents);
            delete_line(m_cursor.line + 1);
            update_total_height();
            return true;
        }
        return false;
    }
    bool concat_forward(void)
    {
        if (m_cursor.line < (long)m_lines.size() - 1) {
            current_line().append(m_lines[m_cursor.line + 1].contents);
            delete_line(m_cursor.line + 1);
            update_total_height();
            return true;
        }
        return false;
    }
    void delete_characters_back(unsigned long amount = 1)
    {
        auto end = m_cursor.line;
        auto moved = move_cursor_left(amount);
        auto start = m_cursor.line;
        auto line_diff = end - start;
        while (line_diff-- > 0) {
            concat_forward();
        }
        current_line().erase(current_line().begin() + m_cursor.col, current_line().begin() + m_cursor.col + moved);
        update_total_height();
    }
    void delete_characters_forward(unsigned long amount = 1)
    {
        auto start = m_cursor.line;
        auto start_col = m_cursor.col;
        move_cursor_right(amount);
        auto end = m_cursor.line;
        auto line_diff = end - start;
        while (line_diff-- > 0) {
            concat_backward();
        }
        current_line().erase(current_line().begin() + start_col, current_line().begin() + m_cursor.col);
        m_cursor.col = start_col;
        update_total_height();
    }
    void delete_words_back(unsigned long amount = 1)
    {
        auto end = m_cursor.line;
        auto moved = move_cursor_word(-amount);
        auto start = m_cursor.line;
        auto line_diff = end - start;
        while (line_diff-- > 0) {
            concat_forward();
        }
        current_line().erase(current_line().begin() + m_cursor.col, current_line().begin() + m_cursor.col + moved);
        update_total_height();
        update_scroll_v(0);
        // update_viewport_to_cursor();
    }
    void delete_words_forward(unsigned long amount = 1)
    {
        auto start = m_cursor.line;
        auto start_col = m_cursor.col;
        move_cursor_word(amount);
        auto end = m_cursor.line;
        auto line_diff = end - start;
        while (line_diff-- > 0) {
            concat_backward();
        }
        current_line().erase(current_line().begin() + start_col, current_line().begin() + m_cursor.col);
        m_cursor.col = start_col;
        update_total_height();
        // update_viewport_to_cursor();
        update_scroll_v(0);
    }
    void insert_character(char_t c)
    {
        current_line().push_back('!');
        std::shift_right(current_line().begin() + m_cursor.col, current_line().end(), 1);
        current_line()[m_cursor.col++] = static_cast<char_t>(c);
    }
    void insert_string(line_t&& str)
    {
        auto const len = str.size();
        current_line().insert(m_cursor.col, str);
        m_cursor.col += len;
        update_total_height();
    }
    void jump_cursor_to_top(bool with_selection = false)
    {
        m_cursor = {};
        if (with_selection)
            update_selection();
        update_viewport_to_cursor();
    }
    void jump_cursor_to_bottom(bool with_selection = false)
    {
        m_cursor = { .line = (long)m_lines.size() - 1, .col = 0 };
        jump_cursor_to_end(with_selection);
        update_viewport_to_cursor();
    }
    void jump_cursor_to_end(bool with_selection = false)
    {
        m_cursor.col = current_line().size();
        if (with_selection)
            update_selection();
    }
    void jump_cursor_to_start(bool with_selection = false)
    {
        m_cursor.col = 0;
        if (with_selection)
            update_selection();
    }
    void insert_newline(void)
    {
        m_lines.resize(m_lines.size() + 1);
        std::shift_right(m_lines.begin() + m_cursor.line + 1, m_lines.end(), 1);
        m_lines.insert(m_lines.begin() + m_cursor.line + 1, {});
        auto& next_line = m_lines[m_cursor.line + 1].contents;
        if (m_cursor.col < (long)current_line().size()) {
            next_line.resize(current_line().size() - m_cursor.col);
            std::copy(current_line().begin() + m_cursor.col, current_line().end(), next_line.begin());
            current_line().erase(current_line().begin() + m_cursor.col, current_line().end());
        }
        m_cursor.line++;
        m_cursor.col = 0;
        update_total_height();
        update_viewport_to_cursor();
    }
    void update_viewport_to_cursor(void)
    {
        const auto current_line_pos = f_line_advance * m_cursor.line;
        if (current_line_pos >= m_bounds.height + m_scroll_v) {
            // update_scroll_v((f_line_advance * (m_cursor.line + 1) - m_scroll_v) - m_bounds.height);
            update_scroll_v(current_line_pos - m_bounds.height + m_scroll_v + f_line_advance);
        }
        if (current_line_pos < m_scroll_v) {
            // m_scroll_v -= (f_line_advance * (m_cursor.line + 1) - m_scroll_v) - m_bounds.height;
            update_scroll_v(current_line_pos - (m_bounds.height + m_scroll_v - f_line_advance));
            // m_scroll_v -= f_line_advance;
        }
    }
    void start_selection(void)
    {
        m_selection = { m_cursor, m_cursor };
    }
    void clear_selection(void)
    {
        m_selection = std::nullopt;
    }
    void draw_buffer(void)
    {
        // for selection checking
        TextBuffer::Cursor _cursor = {};
        Vector2 pos = { m_bounds.x, m_bounds.y - m_scroll_v };
        Rectangle last_c_r = {};
        for (std::size_t linen = 0; linen < get_line_count(); linen++) {
            auto& current_line = m_lines[linen];
            for (size_t col = 0; col < current_line.contents.size();) {
                int csz = 1;
                int c = GetCodepoint((char*)&current_line.contents.data()[col], &csz);
                int idx = GetGlyphIndex(m_font, c);
                const float glyph_width = (m_font.glyphs[idx].advanceX == 0) ? m_font.recs[idx].width * f_scale_factor : m_font.glyphs[idx].advanceX * f_scale_factor;
                if (pos.x + glyph_width > m_bounds.width) {
                    break;
                }
                // float glyph_height = font.recs[idx].height * scale_factor;

                auto r = GetGlyphAtlasRec(m_font, idx);
                if ((long)linen == m_cursor.line && (long)col == m_cursor.col) {
                    DrawRectangleRec(Rectangle {
                                         .x = pos.x,
                                         .y = pos.y,
                                         .width = static_cast<float>(m_glyph_spacing),
                                         .height = f_line_advance },
                        WHITE);
                }
                _cursor.col = col + 1;
                _cursor.line = linen;
                if (get_selection().has_value() && get_selection()->is_cursor_within(_cursor)) {
                    auto rect = Rectangle {
                        .x = pos.x,
                        .y = pos.y,
                        .width = glyph_width + m_glyph_spacing,
                        .height = f_line_advance
                    };
                    DrawRectangleRec(rect, foreground_color);
                    DrawTextCodepoint(m_font, c, pos, m_font_size, background_color);
                } else {
                    DrawTextCodepoint(m_font, c, pos, m_font_size, foreground_color);
                }
                pos.x += glyph_width + m_glyph_spacing;
                col += csz;
                last_c_r = r;
            }
            if (m_cursor.line == (long)linen && m_cursor.col == (long)current_line.contents.size()) {
                auto rect = Rectangle {
                    .x = pos.x,
                    .y = pos.y,
                    .width = static_cast<float>(m_glyph_spacing),
                    .height = f_line_advance
                };
                DrawRectangleRec(rect, foreground_color);
            }
            pos.x = 0;
            pos.y += f_line_advance;
        }
        const float lines_missing = f_total_height - m_bounds.height;
        if (lines_missing > 0)
            draw_vertical_scroll_bar();
    }
    void draw_vertical_scroll_bar(void)
    {
        const auto drawn = m_bounds.height / f_total_height;
        const auto rec = Rectangle {
            .x = m_bounds.width + m_bounds.x - m_v_scroll_bar_width,
            .y = m_bounds.y + m_bounds.height - (drawn * m_bounds.height) - ((f_total_height - m_bounds.height - m_scroll_v) / f_total_height) * m_bounds.height,
            .width = m_v_scroll_bar_width,
            .height = drawn * m_bounds.height,
        };
        DrawRectangleRec(rec, WHITE);
    }
    void update_vertical_scroll_bar(Vector2 p)
    {
        const auto point = (Vector2) {
            .x = p.x - m_bounds.x,
            .y = p.y - m_bounds.y + m_scroll_v,
        };
        const auto drawn = m_bounds.height / f_total_height;
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        }
    }
    void update_buffer_mouse(void)
    {
        const auto p = GetMousePosition();
        const auto point = (Vector2) {
            .x = p.x - m_bounds.x,
            .y = p.y - m_bounds.y + m_scroll_v,
        };
        if (CheckCollisionPointRec(point,
                { .x = m_bounds.width - m_v_scroll_bar_width,
                    .y = 0,
                    .width = m_v_scroll_bar_width,
                    .height = m_bounds.height })) {
            update_vertical_scroll_bar(point);
            return;
        }
        auto c = mouse_as_cursor_position(point);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            clear_selection();
            m_cursor = c.has_value() ? *c : m_cursor;
        }
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            if (!get_selection()) {
                start_selection();
            } else {
                m_cursor = c.has_value() ? *c : m_cursor;
                update_selection();
            }
        }
        const float mouse_move = GetMouseWheelMove();
        if (AnySpecialDown(CONTROL) && mouse_move != 0.0) {
            if (mouse_move > 0)
                increase_font_size();
            else if (mouse_move < 0)
                decrease_font_size();
        } else if (mouse_move != 0.0) {
            update_scroll_v(mouse_move * -5 * (m_font_size / 2.));
        }
    }
    void update_buffer(void)
    {
        update_buffer_mouse();
        const bool shift_down = AnySpecialDown(SHIFT);
        const auto start_pos = m_cursor;
        if (IsKeyPressedOrRepeat(KEY_LEFT)) {
            if (AnySpecialDown(CONTROL)) {
                move_cursor_word(-1, shift_down);
            } else {
                move_cursor_left(1, shift_down);
            }
        }
        if (IsKeyPressedOrRepeat(KEY_H) && AnySpecialDown(CONTROL)) {
            move_cursor_left(1, shift_down);
        }
        if (IsKeyPressedOrRepeat(KEY_A) && AnySpecialDown(CONTROL)) {
            jump_cursor_to_start(shift_down);
        }
        if (IsKeyPressedOrRepeat(KEY_B) && AnySpecialDown(CONTROL)) {
            move_cursor_word(-1, shift_down);
        }
        if (IsKeyPressedOrRepeat(KEY_RIGHT)) {
            if (AnySpecialDown(CONTROL)) {
                move_cursor_word(1, shift_down);
            } else {
                move_cursor_right(1, shift_down);
            }
        }
        if (IsKeyPressedOrRepeat(KEY_L) && AnySpecialDown(CONTROL)) {
            move_cursor_right(1, shift_down);
        }
        if (IsKeyPressedOrRepeat(KEY_E) && AnySpecialDown(CONTROL)) {
            jump_cursor_to_end(shift_down);
        }
        if (IsKeyPressedOrRepeat(KEY_W) && AnySpecialDown(CONTROL)) {
            move_cursor_word(1, shift_down);
        }
        if (IsKeyPressedOrRepeat(KEY_UP)) {
            move_cursor_up(1, shift_down);
        }
        if (IsKeyPressedOrRepeat(KEY_K) && AnySpecialDown(CONTROL)) {
            move_cursor_up(1, shift_down);
        }
        if (IsKeyPressedOrRepeat(KEY_DOWN)) {
            move_cursor_down(1, shift_down);
        }
        if (IsKeyPressedOrRepeat(KEY_J) && AnySpecialDown(CONTROL)) {
            move_cursor_down(1, shift_down);
        }
        if (IsKeyPressedOrRepeat(KEY_END)) {
            jump_cursor_to_end(shift_down);
        }
        if (IsKeyPressedOrRepeat(KEY_HOME)) {
            jump_cursor_to_start(shift_down);
        }
        if (IsKeyPressedOrRepeat(KEY_BACKSPACE)) {
            if (AnySpecialDown(CONTROL))
                delete_words_back();
            else if (get_selection().has_value())
                delete_selection();
            else
                delete_characters_back();
        }
        if (IsKeyPressedOrRepeat(KEY_DELETE)) {
            if (AnySpecialDown(CONTROL))
                delete_words_forward();
            else
                delete_characters_forward();
        }
        if (IsKeyPressedOrRepeat(KEY_ENTER)) {
            insert_newline();
        }
        if (IsKeyPressedOrRepeat(KEY_O) && AnySpecialDown(CONTROL)) {
            insert_newline();
        }
        if (IsKeyPressedOrRepeat(KEY_TAB)) {
            for (auto i = 0; i < 4; i++) {
                insert_character(' ');
            }
        }
        if (IsKeyPressedOrRepeat(KEY_V) && AnySpecialDown(CONTROL)) {
            const char* clipboard = GetClipboardText();
            std::printf("clipboard: %s\n", clipboard);
            TextBuffer::line_t line {};
            for (size_t i = 0; clipboard[i] != '\0'; i++) {
                if (clipboard[i] == '\n') {
                    insert_string(std::move(line));
                    insert_newline();
                    line = {};
                } else if (clipboard[i] == '\r') {
                    continue;
                } else if (clipboard[i] == '\t') {
                    line.append("    ");
                } else {
                    line.push_back(clipboard[i]);
                }
            }
            insert_string(std::move(line));
        }
        if (IsKeyPressedOrRepeat(KEY_C) && AnySpecialDown(CONTROL)) {
            auto sel = copy_selection();
            SetClipboardText(reinterpret_cast<const char*>(sel.data()));
        }
        if (IsKeyPressedOrRepeat(KEY_EQUAL) && AnySpecialDown(SHIFT) && AnySpecialDown(CONTROL)) {
            increase_font_size();
        }
        if (IsKeyPressedOrRepeat(KEY_MINUS) && AnySpecialDown(CONTROL)) {
            decrease_font_size();
        }
        if (IsKeyPressedOrRepeat(KEY_G) && AnySpecialDown(CONTROL)) {
            jump_cursor_to_bottom(shift_down);
        }
        if (IsKeyPressedOrRepeat(KEY_T) && AnySpecialDown(CONTROL)) {
            jump_cursor_to_top(shift_down);
        }
        static unsigned char utfbuf[4] = { 0 };
        int c = 0;
        while ((c = GetCharPressed())) {
            if (get_selection().has_value()) {
                delete_selection();
                clear_selection();
            }
            auto len = utf8::encode_utf8(c, utfbuf);
            for (size_t i = 0; i < len; i++) {
                insert_character(utfbuf[i]);
            }
        }
        if (start_pos != m_cursor && !shift_down)
            clear_selection();
    }

private:
    void update_total_height(void)
    {
        f_total_height = f_line_advance * m_lines.size();
        if (f_total_height <= m_bounds.height)
            m_scroll_v = 0;
    }
    void update_font_measurements(void)
    {
        f_scale_factor = m_font_size / (float)m_font.baseSize;
        f_line_advance = m_font.recs[GetGlyphIndex(m_font, ' ')].height * f_scale_factor;
        update_total_height();
    }
    void update_selection(void)
    {
        m_selection->end = { m_cursor.line, m_cursor.col };
    }
    void clamp_cursor(void)
    {
        m_cursor.line = std::clamp(m_cursor.line, (long)0, (long)m_lines.size() - 1);
        m_cursor.col = std::clamp(m_cursor.col, (long)0, (long)current_line().size());
    }
    std::optional<TextBuffer::Cursor> mouse_as_cursor_position(const Vector2 point)
    {
        if (point.x < 0 || point.x > m_bounds.width || point.y < 0 || point.y > m_bounds.height + m_scroll_v)
            return std::nullopt;
        long linen = point.y == 0 ? 0 : (long)(point.y / f_line_advance) % get_line_count();
        auto& line = m_lines[linen];
        if (line.contents.size() == 0)
            return TextBuffer::Cursor { .line = linen, .col = 0 };
        float advance = 0;
        for (long col = 0; col < (long)line.contents.size();) {
            int csz = 1;
            int c = GetCodepoint((char*)&line.contents.data()[col], &csz);
            int idx = GetGlyphIndex(m_font, c);
            float glyph_width = (m_font.glyphs[idx].advanceX == 0) ? m_font.recs[idx].width * f_scale_factor : m_font.glyphs[idx].advanceX * f_scale_factor;
            // auto r = GetGlyphAtlasRec(font, idx);
            if (point.x >= advance && point.x <= advance + glyph_width + m_glyph_spacing)
                return TextBuffer::Cursor { .line = linen, .col = col };
            advance += glyph_width + m_glyph_spacing;
            col += csz;
        }
        return TextBuffer::Cursor { .line = linen, .col = (long)line.contents.size() };
    }
    void update_scroll_v(float v)
    {
        if (f_total_height <= m_bounds.height)
            return;
        m_scroll_v = std::clamp(m_scroll_v + v, 0.0f, f_total_height - m_bounds.height);
    }
};

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
    InitWindow(800, 600, "bootleg");
    DEFER(CloseWindow());
    auto font = argc == 2 ? (try_load_font(args[1])) : GetFontDefault();
    DEFER(
        if (font.texture.id != GetFontDefault().texture.id)
            UnloadFont(font););
    TextBuffer _text_buffer = { font, bounds };
    _text_buffer.insert_string("Welcome to Bootleg!");
    _text_buffer.set_font_size(50);
    for (auto i = 0; i < 50; i++) {
        _text_buffer.insert_newline();
        _text_buffer.insert_string(std::format("{}", i));
    }
    SetTargetFPS(60);
    while (!WindowShouldClose()) {
        _text_buffer.update_buffer();
        BeginDrawing();
        ClearBackground(BLACK);
        _text_buffer.draw_buffer();
        EndDrawing();
    }
}
