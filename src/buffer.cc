#include "buffer.hpp"
#include <cmath>
#include <print>

namespace bed {

TextBuffer::TextBuffer(Font f, Rectangle bounds)
    : m_font { f }
    , m_bounds { bounds }
{
    update_font_measurements();
    m_v_scroll_bar_width = (m_bounds.width * 0.02f);
}

const Font& TextBuffer::get_font() const
{
    return m_font;
}
void TextBuffer::set_font(Font font)
{
    m_font = font;
    update_font_measurements();
}
int TextBuffer::get_font_size() const
{
    return m_font_size;
}
void TextBuffer::set_font_size(int sz)
{
    m_font_size = sz;
    update_font_measurements();
}
int TextBuffer::get_spacing() const
{
    return m_spacing;
}
void TextBuffer::set_spacing(int s)
{
    m_spacing = s;
    update_font_measurements();
}
float TextBuffer::get_width() const
{
    return m_bounds.width;
}
void TextBuffer::set_width(float w)
{
    auto b = m_bounds;
    b.width = w;
    set_bounds(b);
}
float TextBuffer::get_height() const
{
    return m_bounds.height;
}
void TextBuffer::set_height(float h)
{
    auto b = m_bounds;
    b.height = h;
    set_bounds(b);
}
Rectangle TextBuffer::get_bounds() const
{
    return m_bounds;
}
void TextBuffer::set_bounds(Rectangle b)
{
    m_bounds = b;
    update_viewport_to_cursor();
    update_total_height();
    measure_lines();
}
Vector2 TextBuffer::get_position() const
{
    return { .x = m_bounds.x, .y = m_bounds.y };
}
void TextBuffer::set_position(Vector2 p)
{
    auto b = m_bounds;
    b.x = p.x;
    b.y = p.y;
    set_bounds(b);
}
void TextBuffer::increase_font_size()
{
    m_font_size = std::clamp(m_font_size + 1, 10, 60);
    update_font_measurements();
    update_viewport_to_cursor();
}
void TextBuffer::decrease_font_size()
{
    m_font_size = std::clamp(m_font_size - 1, 10, 60);
    update_font_measurements();
    update_viewport_to_cursor();
}
bool TextBuffer::is_cursor_at_begining(void)
{
    return (m_cursor.col == 0 && m_cursor.line == 0);
}
bool TextBuffer::is_cursor_at_end(void)
{
    return (m_cursor.col == (long)m_lines.back().contents.size() && m_cursor.line == (long)m_lines.size() - 1);
}
void TextBuffer::toggle_wrap_lines(void)
{
    m_wrap_lines = !m_wrap_lines;
}
bool TextBuffer::is_wrapping_lines(void) const
{
    return m_wrap_lines;
}
TextBuffer::line_t& TextBuffer::current_line(void)
{
    return m_lines[m_cursor.line].contents;
}
const TextBuffer::line_t& TextBuffer::current_line(void) const
{
    return m_lines[m_cursor.line].contents;
}
const std::optional<TextBuffer::Selection>& TextBuffer::get_selection(void)
{
    return m_selection;
}
std::optional<TextBuffer::line_t::value_type> TextBuffer::get_char_under_cursor(void) const
{
    if (m_cursor.col <= 0 || m_cursor.col > (long)current_line().size())
        return std::nullopt;
    return current_line()[m_cursor.col - 1];
}
// if | is the cursor then x is the character
//
// a|x
std::optional<TextBuffer::line_t::value_type> TextBuffer::get_char_after_cursor(void) const
{
    if (current_line().empty() || m_cursor.col >= (long)current_line().size())
        return std::nullopt;
    return current_line()[m_cursor.col];
}
size_t TextBuffer::get_line_count(void) const
{
    return m_lines.size();
}
void TextBuffer::delete_selection(void)
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
TextBuffer::line_t TextBuffer::copy_selection(void)
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
TextBuffer::line_t TextBuffer::cut_selection(void)
{
    auto selection = copy_selection();
    delete_selection();
    return selection;
}
long TextBuffer::move_cursor_word(long amount, bool with_selection)
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
long TextBuffer::move_cursor_left(long amount, bool with_selection)
{
    return move_cursor_h(-amount, with_selection);
}
long TextBuffer::move_cursor_right(long amount, bool with_selection)
{
    return move_cursor_h(amount, with_selection);
}
// returns how many positions the cursor moved (across lines, and counted in bytes)
long TextBuffer::move_cursor_h(long amount, bool with_selection)
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
    update_scroll_h();
    return moved;
}
long TextBuffer::count_chars_to_cursor_in_line(void)
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
long TextBuffer::move_cursor_v(long amount, bool with_selection)
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
void TextBuffer::move_cursor_down(long amount, bool with_selection)
{
    move_cursor_v(amount, with_selection);
}
void TextBuffer::move_cursor_up(long amount, bool with_selection)
{
    move_cursor_v(-amount, with_selection);
}
void TextBuffer::delete_line(size_t line_num)
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
void TextBuffer::delete_lines(size_t start, size_t end)
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
bool TextBuffer::concat_backward(void)
{
    if (m_cursor.line > 0) {
        m_cursor.line--;
        jump_cursor_to_end();
        current_line().append(m_lines[m_cursor.line + 1].contents);
        delete_line(m_cursor.line + 1);
        update_total_height();
        update_scroll_h();
        return true;
    }
    return false;
}
bool TextBuffer::concat_forward(void)
{
    if (m_cursor.line < (long)m_lines.size() - 1) {
        current_line().append(m_lines[m_cursor.line + 1].contents);
        delete_line(m_cursor.line + 1);
        update_total_height();
        update_scroll_h();
        return true;
    }
    return false;
}
void TextBuffer::delete_characters_back(unsigned long amount)
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
    update_scroll_h();
}
void TextBuffer::delete_characters_forward(unsigned long amount)
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
    update_scroll_h();
}
void TextBuffer::delete_words_back(unsigned long amount)
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
    update_scroll_h();
}
void TextBuffer::delete_words_forward(unsigned long amount)
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
    update_scroll_v(0);
    update_scroll_h();
}
void TextBuffer::insert_character(char_t c)
{
    current_line().push_back('!');
    std::shift_right(current_line().begin() + m_cursor.col, current_line().end(), 1);
    current_line()[m_cursor.col++] = static_cast<char_t>(c);
    update_scroll_h();
    measure_line(m_lines[m_cursor.line]);
}
void TextBuffer::insert_string(line_t&& str)
{
    auto const len = str.size();
    current_line().insert(m_cursor.col, str);
    m_cursor.col += len;
    update_total_height();
    update_viewport_to_cursor();
    measure_line(m_lines[m_cursor.line]);
}
void TextBuffer::jump_cursor_to_top(bool with_selection)
{
    m_cursor = {};
    if (with_selection)
        update_selection();
    update_viewport_to_cursor();
}
void TextBuffer::jump_cursor_to_bottom(bool with_selection)
{
    m_cursor = { .line = (long)m_lines.size() - 1, .col = 0 };
    jump_cursor_to_end(with_selection);
    update_viewport_to_cursor();
}
void TextBuffer::jump_cursor_to_end(bool with_selection)
{
    m_cursor.col = current_line().size();
    if (with_selection)
        update_selection();
    update_scroll_h();
}
void TextBuffer::jump_cursor_to_start(bool with_selection)
{
    m_cursor.col = 0;
    if (with_selection)
        update_selection();
    update_scroll_h();
}
void TextBuffer::insert_newline(void)
{
    m_lines.resize(m_lines.size() + 1);
    std::shift_right(m_lines.begin() + m_cursor.line + 1, m_lines.end(), 1);
    m_lines.insert(m_lines.begin() + m_cursor.line + 1, {});
    auto& next_line = m_lines[m_cursor.line + 1].contents;
    if (m_cursor.col < (long)current_line().size()) {
        next_line.resize(current_line().size() - m_cursor.col);
        std::copy(current_line().begin() + m_cursor.col, current_line().end(), next_line.begin());
        current_line().erase(current_line().begin() + m_cursor.col, current_line().end());
        measure_line(m_lines[m_cursor.line]);
    }
    m_cursor.line++;
    m_cursor.col = 0;
    update_total_height();
    update_viewport_to_cursor();
    measure_line(m_lines[m_cursor.line]);
}
/// this function ensures that the viewport contains the cursor (the cursor is visible on the screen)
void TextBuffer::update_viewport_to_cursor(void)
{
    const auto current_line_pos = f_line_advance * m_cursor.line;
    if (current_line_pos >= m_bounds.height + m_scroll_v || current_line_pos < m_scroll_v) {
        update_scroll_v(current_line_pos - (m_bounds.height + m_scroll_v - f_line_advance));
    }
    update_scroll_h();
}
void TextBuffer::start_selection(void)
{
    m_selection = { m_cursor, m_cursor };
}
void TextBuffer::clear_selection(void)
{
    m_selection = std::nullopt;
}
float TextBuffer::get_glyph_width(const Font& font, int codepoint) const
{
    int idx = GetGlyphIndex(font, codepoint);
    float glyph_width = (font.glyphs[idx].advanceX == 0)
        ? m_font.recs[idx].width * f_scale_factor
        : m_font.glyphs[idx].advanceX * f_scale_factor;
    return glyph_width;
}
float TextBuffer::measure_line_till_cursor(void)
{
    if (current_line().size() == 0)
        return 0;
    float advance = 0.0;
    for (long col = 0; col <= m_cursor.col;) {
        int csz = 1;
        int c = GetCodepoint(&current_line().data()[col], &csz);
        float glyph_width = get_glyph_width(m_font, c);
        advance += glyph_width + m_glyph_spacing;
        col += csz;
    }
    return advance;
}
void TextBuffer::measure_line(Line& line)
{
    Vector2 dims = {};
    float width_max = 0.0;
    line.lines_when_wrapped = 1;
    for (long col = 0; col < (long)line.contents.size();) {
        int csz = 1;
        int c = GetCodepoint((char*)&line.contents.data()[col], &csz);
        float glyph_width = get_glyph_width(m_font, c);
        auto tmp = glyph_width + m_glyph_spacing;
        if(dims.x + tmp >= m_bounds.width){
            dims.y += f_line_advance;
            line.lines_when_wrapped++;
            width_max = std::max(width_max, dims.x);
            dims.x = 0.0;
        }
        dims.x += tmp;
        col += csz;
    }
    dims.x = width_max;
    line.dims = dims;
}
void TextBuffer::measure_lines(void)
{
    f_total_width = 0.0;
    for (auto& line_data : m_lines) {
        measure_line(line_data);
        if (line_data.dims->x > f_total_width)
            f_total_width = line_data.dims->x;
    }
}
void TextBuffer::draw(void)
{
    DrawRectangleRec(m_bounds, background_color);
    // for selection checking
    TextBuffer::Cursor _cursor = {};
    Vector2 pos = { m_bounds.x - (m_wrap_lines ? 0 : m_scroll_h), m_bounds.y - m_scroll_v };
    for (std::size_t linen = 0; linen < get_line_count(); linen++) {
        auto& current_line = m_lines[linen];
        for (size_t col = 0; col < current_line.contents.size();) {
            bool skip_draws = !CheckCollisionPointRec(pos, m_bounds);
            int csz = 1;
            int c = GetCodepoint((char*)&current_line.contents.data()[col], &csz);
            const float glyph_width = get_glyph_width(m_font, c);
            //wrap the line if it goes out of bounds
            if (m_wrap_lines && pos.x >= m_bounds.x + m_bounds.width - glyph_width) {
                pos.x = m_bounds.x;
                pos.y += f_line_advance;
                skip_draws = false;
            }
            if ((long)linen == m_cursor.line && (long)col == m_cursor.col && !skip_draws) {
                auto cursor_line = Rectangle {
                    .x = pos.x,
                    .y = pos.y,
                    .width = static_cast<float>(m_glyph_spacing),
                    .height = f_line_advance
                };
                DrawRectangleRec(cursor_line, WHITE);
            }
            _cursor.col = col + 1;
            _cursor.line = linen;
            if (get_selection().has_value() && get_selection()->is_cursor_within(_cursor) && !skip_draws) {
                auto glyph = Rectangle {
                    .x = pos.x,
                    .y = pos.y,
                    .width = glyph_width + m_glyph_spacing,
                    .height = f_line_advance
                };
                DrawRectangleRec(glyph, foreground_color);
                DrawTextCodepoint(m_font, c, pos, m_font_size, background_color);
            } else if (!skip_draws) {
                DrawTextCodepoint(m_font, c, pos, m_font_size, foreground_color);
            }
            pos.x += glyph_width + m_glyph_spacing;
            col += csz;
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
        pos.x = m_bounds.x - (m_wrap_lines ? 0 : m_scroll_h);
        pos.y += f_line_advance;
    }
}
void TextBuffer::draw_vertical_scroll_bar(void)
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
void TextBuffer::update_buffer_mouse(void)
{
    const auto p = GetMousePosition();
    if (!CheckCollisionPointRec(p, m_bounds))
        return;
    const auto point = (Vector2) {
        .x = p.x - m_bounds.x + (m_wrap_lines ? 0 : m_scroll_h),
        .y = p.y - m_bounds.y + m_scroll_v,
    };
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        auto c = mouse_as_cursor_position(point);
        clear_selection();
        m_cursor = c.has_value() ? *c : m_cursor;
    }
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        auto c = mouse_as_cursor_position(point);
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
void TextBuffer::update_buffer(void)
{
    const bool shift_down = AnySpecialDown(SHIFT);
    if (shift_down && !m_selection)
        start_selection();

    update_buffer_mouse();
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
    if (IsKeyPressedOrRepeat(KEY_X) && AnySpecialDown(CONTROL)) {
        auto sel = cut_selection();
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

void TextBuffer::update_scroll_h(void)
{
    m_cursor_dist = measure_line_till_cursor();
    if (m_cursor_dist > m_bounds.width) {
        m_scroll_h = m_cursor_dist - m_bounds.width;
    } else
        m_scroll_h = 0.0;
}
void TextBuffer::update_total_height(void)
{
    f_total_height = f_line_advance * m_lines.size();
    if (f_total_height <= m_bounds.height)
        m_scroll_v = 0;
}
void TextBuffer::update_font_measurements(void)
{
    f_scale_factor = m_font_size / (float)m_font.baseSize;
    f_line_advance = m_font.recs[GetGlyphIndex(m_font, ' ')].height * f_scale_factor;
    update_total_height();
}
void TextBuffer::update_selection(void)
{
    m_selection->end = { m_cursor.line, m_cursor.col };
}
void TextBuffer::clamp_cursor(void)
{
    m_cursor.line = std::clamp(m_cursor.line, (long)0, (long)m_lines.size() - 1);
    m_cursor.col = std::clamp(m_cursor.col, (long)0, (long)current_line().size());
}
std::optional<TextBuffer::Cursor> TextBuffer::mouse_as_cursor_position(Vector2 point)
{
    // measure_lines();
    std::println("{{ x: {}, y: {} }}", point.x, point.y);
    std::println("f_line_advance: {}", f_line_advance);
    if (point.x < 0 || point.x > m_bounds.width + m_scroll_h || point.y < 0 || point.y > m_bounds.height + m_scroll_v)
        return std::nullopt;
    long linenum = 0;
    if (m_wrap_lines) {
        auto last_line_end = -m_scroll_v;
        for (auto i = 0; i < (long)get_line_count(); i++) {
            std::println("lines_when_wrapped: {}", m_lines[i].lines_when_wrapped);
            auto this_line_end = last_line_end + (m_lines[i].lines_when_wrapped * f_line_advance);
            if (point.y <= this_line_end) {
                break;
            }
            last_line_end = this_line_end;
            linenum++;
        }
        linenum = linenum >= (int)get_line_count() ? get_line_count() - 1 : linenum;
        int y_in_line = (point.y - last_line_end) / f_line_advance;
        std::println("point.y : {}, last_line_end : {}", point.y, last_line_end);
        std::println("y_in_line : {}", y_in_line);
        std::println("linenum : {}", linenum);
        point.y = y_in_line;
    } else {
        linenum = point.y == 0 ? 0 : (long)(point.y / f_line_advance) % get_line_count();
    }
    auto& line = m_lines[linenum];
    if (line.contents.size() == 0)
        return TextBuffer::Cursor { .line = linenum, .col = 0 };
    float advance = 0;
    int line_in_line = 0;
    for (long col = 0; col < (long)line.contents.size();) {
        int csz = 1;
        int c = GetCodepoint((char*)&line.contents.data()[col], &csz);
        float glyph_width = get_glyph_width(m_font, c);
        auto is_in_line_with_wrapping = m_wrap_lines ? (line_in_line == point.y) : true;
        if (point.x >= advance && point.x <= advance + glyph_width + m_glyph_spacing && is_in_line_with_wrapping)
            return TextBuffer::Cursor { .line = linenum, .col = col };
        advance += glyph_width + m_glyph_spacing;
        col += csz;
        if (advance >= m_bounds.width - glyph_width && m_wrap_lines) {
            advance = 0.0;
            line_in_line++;
        }
    }
    return TextBuffer::Cursor { .line = linenum, .col = (long)line.contents.size() };
}
void TextBuffer::update_scroll_v(float v)
{
    if (f_total_height <= m_bounds.height)
        return;
    m_scroll_v = std::clamp(m_scroll_v + v, 0.0f, f_total_height - m_bounds.height);
}
TextBuffer::line_t TextBuffer::get_contents_as_string(void) const
{
    line_t ret = {};
    for (const auto& line : m_lines) {
        ret.append(line.contents);
        ret.push_back('\n');
    }
    return ret;
}
}
