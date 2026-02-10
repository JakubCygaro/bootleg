#include "buffer.hpp"
#include <iostream>
namespace bed {
using tit = TextBuffer::text_buffer_iterator;
tit::text_buffer_iterator(const std::vector<TextBuffer::Line>* lines)
    : m_lines(lines)
    , m_sz(lines->size())
    , m_current_line_len((*m_lines)[m_line].contents.size())
    {};
tit tit::end(const std::vector<TextBuffer::Line>* lines)
{
    auto t = text_buffer_iterator(lines);
    t.m_line = t.m_sz;
    return t;
}
TextBuffer::char_t tit::operator*() const
{
    if (m_col == m_current_line_len) {
        std::cout << "passing newline" << std::endl;
        return '\n';
    }
    return (*m_lines)[m_line].contents[m_col];
}
void tit::operator++()
{
    if (m_line >= m_sz)
        return;
    m_col++;
    if (m_col > m_current_line_len) {
        m_line++;
        m_col = 0;
        if (m_line < m_sz)
            m_current_line_len = (*m_lines)[m_line].contents.size();
    }
}
void tit::operator++(int)
{
    ++(*this);
}
bool tit::operator==(const tit& other) const
{
    if (m_lines != other.m_lines)
        return false;
    return (other.m_line >= m_sz && m_line >= m_sz) || (other.m_line == m_line && other.m_col == m_col);
}
bool tit::operator!=(const tit& other) const
{
    return !(*this == other);
}
TextBuffer::Cursor tit::current_cursor_pos(void) const
{
    return TextBuffer::Cursor { (long)m_line, (long)m_col };
}
}
