#include "buffer.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utf8.hpp>

namespace bed {

TextBuffer::TextBuffer(Font f, Rectangle bounds)
    : m_font { f }
    , m_bounds { bounds }
{
    update_font_measurements();
    m_v_scroll_bar_width = (m_bounds.width * 0.02f);
}

void TextBuffer::_set_font(std::optional<Font> font, std::optional<int> sz, std::optional<int> spacing)
{
    if (font)
        m_font = *font;
    if (sz)
        m_font_size = *sz;
    if (spacing)
        m_spacing = *spacing;
    update_font_measurements();
    measure_lines();
    update_viewport_to_cursor();
}
const Font& TextBuffer::get_font() const
{
    return m_font;
}
void TextBuffer::set_font(Font font)
{
    _set_font(font);
}
int TextBuffer::get_font_size() const
{
    return m_font_size;
}
void TextBuffer::set_font_size(int sz)
{
    _set_font(std::nullopt, sz);
}
int TextBuffer::get_spacing() const
{
    return m_spacing;
}
void TextBuffer::set_spacing(int s)
{
    _set_font(std::nullopt, std::nullopt, s);
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
    _set_font(std::nullopt, std::clamp(m_font_size + 1, 10, 60));
}
void TextBuffer::decrease_font_size()
{
    _set_font(std::nullopt, std::clamp(m_font_size - 1, 10, 60));
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
    if (m_wrap_lines)
        measure_lines();
}
void TextBuffer::wrap_lines(bool b)
{
    m_wrap_lines = b;
}
bool TextBuffer::is_wrapping_lines(void) const
{
    return m_wrap_lines;
}
void TextBuffer::toggle_readonly(void)
{
    m_readonly = !m_readonly;
}
bool TextBuffer::is_readonly(void) const
{
    return m_readonly;
}
TextBuffer::line_t& TextBuffer::current_line(void)
{
    return m_lines[m_cursor.line].contents;
}
bool TextBuffer::has_focus(void) const
{
    return m_has_focus;
}
size_t TextBuffer::get_line_number(void) const
{
    return m_cursor.line;
}
void TextBuffer::toggle_cursor(void)
{
    m_draw_cursor = !m_draw_cursor;
}
bool TextBuffer::is_cursor_visible(void) const
{
    return m_draw_cursor;
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
    update_syntax();
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
    const auto under_check = [&](std::optional<char_t>& u) {
        if (!u.has_value())
            return true;
        auto uc = u.value();
        return (bool)std::isspace(uc) || ((bool)!std::isalnum(uc) && utf8::get_utf8_bytes_len(uc) != -1);
    };
    const auto after_check = [](std::optional<char_t>& a) {
        if (!a.has_value())
            return false;
        auto ac = a.value();
        return ((bool)std::isalnum(ac) || utf8::get_utf8_bytes_len(ac) != 1) && !std::isspace(ac);
    };
    const auto is_punct = [](std::optional<char_t>& a) {
        if (!a)
            return false;
        return (bool)std::ispunct(a.value());
    };
    auto moved = 0;
    auto i = 0;
    while (i < amount_abs && !end_check()) {
        moved += _move_cursor(MoveDir::HORIZONTAL, inc, with_selection);
        auto u = get_char_under_cursor();
        auto a = get_char_after_cursor();
        if (is_punct(a) || (under_check(u) && after_check(a)))
            i++;
    }
    return moved;
}
long TextBuffer::move_cursor_left(long amount, bool with_selection)
{
    return _move_cursor(MoveDir::HORIZONTAL, -amount, with_selection);
}
long TextBuffer::move_cursor_right(long amount, bool with_selection)
{
    return _move_cursor(MoveDir::HORIZONTAL, amount, with_selection);
}
// returns how many positions the cursor moved (across lines, and counted in bytes)
long TextBuffer::move_cursor_h(long amount)
{
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
            break;
        } else if (m_cursor.col > (long)current_line().size() && m_cursor.line == (long)m_lines.size() - 1) {
            m_cursor.col = (long)current_line().size();
            break;
        } else {
            moved++;
        }
    }
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
void TextBuffer::_set_cursor(long line, long column, bool with_selection){
    if (with_selection && !m_selection)
        start_selection();
    m_cursor.line = line;
    m_cursor.col = column;
    if (with_selection)
        update_selection();
}
long TextBuffer::_move_cursor(MoveDir dir, long amount, bool with_selection)
{
    if (!amount)
        return amount;
    if (with_selection && !m_selection)
        start_selection();
    long ret = 0;
    switch (dir) {
    case MoveDir::HORIZONTAL:
        ret = move_cursor_h(amount);
        break;
    case MoveDir::VERTICAL:
        ret = move_cursor_v(amount);
        break;
    }
    if (with_selection)
        update_selection();
    // m_do_common_updates = true;
    update_viewport_to_cursor();
    update_scroll_h();
    return ret;
}
long TextBuffer::move_cursor_v(long amount)
{
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
    else{
        move_cursor_right(chars);
    }
    return amount;
}
void TextBuffer::move_cursor_down(long amount, bool with_selection)
{
    _move_cursor(MoveDir::VERTICAL, amount, with_selection);
}
void TextBuffer::move_cursor_up(long amount, bool with_selection)
{
    _move_cursor(MoveDir::VERTICAL, -amount, with_selection);
}
void TextBuffer::delete_line(size_t line_num)
{
    delete_lines(line_num, line_num);
}
void TextBuffer::delete_lines(size_t start, size_t end)
{
    if (m_lines.size() == 1) {
        m_lines[0].contents.erase();
        return;
    }
    m_lines.erase(m_lines.begin() + start, m_lines.begin() + end + 1);
    clamp_cursor();
    // update_total_height();
    // update_viewport_to_cursor();
    // update_syntax();
    m_do_common_updates = true;
}
void TextBuffer::clear(void)
{
    m_lines.clear();
    m_lines.push_back({});
    m_cursor = {};
    measure_lines();
    update_total_height();
    update_viewport_to_cursor();
    m_syntax_data.clear();
}
bool TextBuffer::_concat(ConcatDir dir)
{
    bool ret = false;
    switch (dir) {
    case ConcatDir::BACKWARD:
        if (m_cursor.line > 0) {
            m_cursor.line--;
            jump_cursor_to_end();
            current_line().append(m_lines[m_cursor.line + 1].contents);
            delete_line(m_cursor.line + 1);
            ret = true;
        }
        break;
    case ConcatDir::FORWARD:
        if (m_cursor.line < (long)m_lines.size() - 1) {
            current_line().append(m_lines[m_cursor.line + 1].contents);
            delete_line(m_cursor.line + 1);
            ret = true;
        }
        break;
    }
    // update_total_height();
    // update_scroll_h();
    // update_syntax();
    m_do_common_updates = true;
    measure_line(m_lines[m_cursor.line]);
    return ret;
}
bool TextBuffer::concat_backward(void)
{
    return _concat(ConcatDir::BACKWARD);
}
bool TextBuffer::concat_forward(void)
{
    return _concat(ConcatDir::FORWARD);
}
void TextBuffer::_delete_characters(DeleteDir dir, unsigned long amount)
{
    if (!amount)
        return;
    switch (dir) {
    case DeleteDir::BACKWARD: {
        auto end = m_cursor.line;
        auto moved = move_cursor_left(amount);
        auto start = m_cursor.line;
        auto line_diff = end - start;
        while (line_diff-- > 0) {
            concat_forward();
        }
        current_line().erase(current_line().begin() + m_cursor.col, current_line().begin() + m_cursor.col + moved);
    } break;
    case DeleteDir::FORWARD: {
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
    } break;
    }
    // update_total_height();
    // update_scroll_h();
    // update_syntax();
    m_do_common_updates = true;
    measure_line(m_lines[m_cursor.line]);
}
void TextBuffer::delete_characters_back(unsigned long amount)
{
    _delete_characters(DeleteDir::BACKWARD, amount);
}
void TextBuffer::delete_characters_forward(unsigned long amount)
{
    _delete_characters(DeleteDir::FORWARD, amount);
}
void TextBuffer::_delete_words(DeleteDir dir, unsigned long amount)
{
    switch (dir) {
    case DeleteDir::BACKWARD: {
        auto end = m_cursor.line;
        auto moved = move_cursor_word(-amount);
        auto start = m_cursor.line;
        auto line_diff = end - start;
        while (line_diff-- > 0) {
            concat_forward();
        }
        current_line().erase(current_line().begin() + m_cursor.col, current_line().begin() + m_cursor.col + moved);
    } break;
    case DeleteDir::FORWARD: {

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
    } break;
    }
    // update_total_height();
    // update_scroll_h();
    // update_syntax();
    m_do_common_updates = true;
    // update_scroll_v(0);
    measure_line(m_lines[m_cursor.line]);
}
void TextBuffer::delete_words_back(unsigned long amount)
{
    _delete_words(DeleteDir::BACKWARD, amount);
}
void TextBuffer::delete_words_forward(unsigned long amount)
{
    _delete_words(DeleteDir::FORWARD, amount);
}
void TextBuffer::insert_character(char_t c)
{
    current_line().push_back('!');
    std::shift_right(current_line().begin() + m_cursor.col, current_line().end(), 1);
    current_line()[m_cursor.col++] = static_cast<char_t>(c);
    measure_line(m_lines[m_cursor.line]);
    m_do_common_updates = true;
    // update_scroll_h();
    // update_syntax();
}
void TextBuffer::insert_string(line_t&& str)
{
    std::string line = {};
    auto start = m_cursor.line;
    for (const auto c : str) {
        if (c == '\n') {
            current_line().insert(m_cursor.col, line);
            m_cursor.col += line.length();
            insert_newline();
            line = {};
        } else if (c == '\r') {
            continue;
        } else if (c == '\t') {
            line.append("    ");
        } else {
            line.push_back(c);
        }
    }
    current_line().insert(m_cursor.col, line);
    m_cursor.col += line.length();

    auto end = m_cursor.line;
    for (auto i = start; i <= end; i++) {
        measure_line(m_lines[i]);
    }
    // update_total_height();
    // update_viewport_to_cursor();
    // update_syntax();
    m_do_common_updates = true;
}
void TextBuffer::insert_line(line_t&& str)
{
    auto const len = str.size();
    current_line().insert(m_cursor.col, str);
    m_cursor.col += len;
    update_total_height();
    update_viewport_to_cursor();
    measure_line(m_lines[m_cursor.line]);
    insert_newline();
    measure_line(m_lines[m_cursor.line]);
    update_syntax();
}
void TextBuffer::jump_cursor_to_top(bool with_selection)
{
    _move_cursor(MoveDir::VERTICAL, -m_cursor.line, with_selection);
    jump_cursor_to_start(with_selection);
}
void TextBuffer::jump_cursor_to_bottom(bool with_selection)
{
    _move_cursor(MoveDir::VERTICAL, get_line_count() - 1 - m_cursor.line, with_selection);
    jump_cursor_to_end(with_selection);
}
void TextBuffer::jump_cursor_to_end(bool with_selection)
{
    _set_cursor(m_cursor.line, current_line().size(), with_selection);
    m_do_common_updates = true;
}
void TextBuffer::jump_cursor_to_start(bool with_selection)
{
    _set_cursor(m_cursor.line, 0, with_selection);
    m_do_common_updates = true;
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
    // update_total_height();
    // update_viewport_to_cursor();
    // update_syntax();
    m_do_common_updates = true;
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
void TextBuffer::update_syntax(void)
{
    m_syntax_data.clear();
    if (m_syntax_parse_fn)
        m_syntax_parse_fn(m_syntax_data, this->begin(), this->end());
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
        if (dims.x + tmp >= m_bounds.width) {
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
    BeginScissorMode(m_bounds.x, m_bounds.y, m_bounds.width, m_bounds.height);
    DrawRectangleRec(m_bounds, background_color);
    // for selection checking
    TextBuffer::Cursor _cursor = {};
    Vector2 pos = { m_bounds.x - (m_wrap_lines ? 0 : m_scroll_h), m_bounds.y - m_scroll_v };
    Color fc = foreground_color;
    for (std::size_t linen = 0; linen < get_line_count(); linen++) {
        auto& current_line = m_lines[linen];
        for (size_t col = 0; col < current_line.contents.size();) {
            int csz = 1;
            int c = GetCodepoint((char*)&current_line.contents.data()[col], &csz);
            const float glyph_width = get_glyph_width(m_font, c);
            // bool skip_draws = m_wrap_lines ? !CheckCollisionPointRec({ pos.x + glyph_width, pos.y }, m_bounds) || pos.x < m_bounds.x
            //                                // : !CheckCollisionPointRec({ pos.x, pos.y }, m_bounds);
            //                                : false;
            bool skip_draws = m_wrap_lines ? pos.x + glyph_width + m_glyph_spacing > m_bounds.x + m_bounds.width
                                           : false;
            // wrap the line if it goes out of bounds
            if (m_wrap_lines && skip_draws) {
                pos.x = m_bounds.x;
                pos.y += f_line_advance;
                skip_draws = false;
            }
            if ((long)linen == m_cursor.line && (long)col == m_cursor.col && !skip_draws && m_draw_cursor) {
                auto cursor_line = Rectangle {
                    .x = pos.x,
                    .y = pos.y,
                    .width = static_cast<float>(m_glyph_spacing),
                    .height = f_line_advance
                };
                DrawRectangleRec(cursor_line, foreground_color);
            }
            const auto current_cursor = Cursor { static_cast<long>(linen), static_cast<long>(col) };
            if (m_syntax_parse_fn && m_syntax_data.contains(current_cursor)) {
                fc = m_syntax_data[current_cursor];
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
            }
            else if (!skip_draws) {
                DrawTextCodepoint(m_font, c, pos, m_font_size, fc);
            }
            pos.x += glyph_width + m_glyph_spacing;
            col += csz;
        }
        if (m_cursor.line == (long)linen && m_cursor.col == (long)current_line.contents.size() && m_draw_cursor) {
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
    EndScissorMode();
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
    const auto inbounds = CheckCollisionPointRec(p, m_bounds);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        m_has_focus = inbounds;
    }
    if (!inbounds || !m_has_focus)
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
    if (!m_has_focus)
        return;
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
    if (IsKeyPressedOrRepeat(KEY_BACKSPACE) && !m_readonly) {
        if (AnySpecialDown(CONTROL))
            delete_words_back();
        else if (get_selection().has_value())
            delete_selection();
        else
            delete_characters_back();
    }
    if (IsKeyPressedOrRepeat(KEY_DELETE) && !m_readonly) {
        if (AnySpecialDown(CONTROL))
            delete_words_forward();
        else
            delete_characters_forward();
    }
    if ((IsKeyPressedOrRepeat(KEY_KP_ENTER) || IsKeyPressedOrRepeat(KEY_ENTER)) && !m_readonly) {
        insert_newline();
    }
    if (IsKeyPressedOrRepeat(KEY_O) && AnySpecialDown(CONTROL) && !m_readonly) {
        insert_newline();
    }
    if (IsKeyPressedOrRepeat(KEY_TAB) && !m_readonly) {
        for (auto i = 0; i < 4; i++) {
            insert_character(' ');
        }
    }
    if (IsKeyPressedOrRepeat(KEY_V) && AnySpecialDown(CONTROL) && !m_readonly) {
        const char* clipboard = GetClipboardText();
        TextBuffer::line_t line { clipboard };
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
    while ((c = GetCharPressed()) && !m_readonly) {
        if (get_selection().has_value()) {
            delete_selection();
            clear_selection();
        }
        auto len = utf8::encode_utf8(c, utfbuf);
        for (size_t i = 0; i < len; i++) {
            insert_character(utfbuf[i]);
        }
    }
    if (m_do_common_updates) {
        m_do_common_updates = false;
        update_total_height();
        update_viewport_to_cursor();
        update_scroll_h();
        update_syntax();
        update_scroll_v(0);
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
    if(!m_wrap_lines)
        f_total_height = f_line_advance * m_lines.size();
    else {
        f_total_height = 0;
        for(const auto& l : m_lines){
            f_total_height += l.lines_when_wrapped * f_line_advance;
        }
    }
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
    if (point.x < 0 || point.x > m_bounds.width + m_scroll_h || point.y < 0 || point.y > m_bounds.height + m_scroll_v)
        return std::nullopt;
    long linenum = 0;
    if (m_wrap_lines) {
        point.y -= m_scroll_v;
        auto last_line_end = -m_scroll_v;
        for (auto i = 0; i < (long)get_line_count(); i++) {
            auto this_line_end = last_line_end + (m_lines[i].lines_when_wrapped * f_line_advance);
            if (point.y <= this_line_end) {
                break;
            }
            last_line_end = this_line_end;
            linenum++;
        }
        linenum = linenum >= (int)get_line_count() ? get_line_count() - 1 : linenum;
        int y_in_line = (point.y - last_line_end) / f_line_advance;
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
TextBuffer::text_buffer_iterator TextBuffer::create_begin_iterator(void) const
{
    return text_buffer_iterator(&m_lines);
}
TextBuffer::text_buffer_iterator TextBuffer::create_end_iterator(void) const
{
    return text_buffer_iterator::end(&m_lines);
}
TextBuffer::text_buffer_iterator TextBuffer::begin(void) const
{
    return create_begin_iterator();
}
TextBuffer::text_buffer_iterator TextBuffer::end(void) const
{
    return create_end_iterator();
}
void TextBuffer::set_syntax_parser(process_syntax_fn fn)
{
    m_syntax_parse_fn = fn;
}
}
