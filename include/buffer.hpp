#ifndef BUFFER_HPP
#define BUFFER_HPP

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <optional>
#include <raylib.h>
#include <string>
#include <unordered_map>
#include <vector>

#define IsKeyPressedOrRepeat(KEY) (IsKeyPressed(KEY) || IsKeyPressedRepeat(KEY))
#define AnySpecialDown(SPECIAL_KEY) (IsKeyDown(KEY_LEFT_##SPECIAL_KEY) || IsKeyDown(KEY_RIGHT_##SPECIAL_KEY))

namespace bed {
struct TextBuffer;

struct TextBuffer {
    using char_t = char;
    using line_t = std::basic_string<char_t>;
    struct Line {
        line_t contents {};
        std::optional<Vector2> dims {};
        int lines_when_wrapped = 1;
    };
    struct Cursor {
        long line {};
        long col {};
        inline bool operator==(const Cursor& b) const noexcept
        {
            return (this->line == b.line) && (this->col == b.col);
        }
        inline bool operator!=(const Cursor& b) const noexcept
        {
            return !(b == *this);
        }
        inline bool operator<(const Cursor& b) const noexcept
        {
            return (this->line < b.line) || (this->line == b.line && this->col < b.col);
        }
        inline bool operator>(const Cursor& b) const noexcept
        {
            return b < *this;
        }
        inline bool operator<=(const Cursor& b) const noexcept
        {
            return (*this < b) || (*this == b);
        }
        inline bool operator>=(const Cursor& b) const noexcept
        {
            return (*this > b) || (*this == b);
        }
        struct HashFn {
            std::size_t operator()(const Cursor& cursor) const noexcept
            {
                auto h1 = std::hash<long>()(cursor.line);
                auto h2 = std::hash<long>()(cursor.col);
                return h1 ^ h2;
            }
        };
    };
    using syntax_data_t = std::unordered_map<Cursor, Color, Cursor::HashFn>;
    struct Selection {
        Cursor start {};
        Cursor end {};
        inline bool is_cursor_within(const Cursor& c) const noexcept
        {
            if (this->start <= this->end)
                return (c > this->start && c <= this->end);
            else
                return (c > this->end && c <= this->start);
        }
    };
    class text_buffer_iterator;
    using process_syntax_fn = std::function<void(syntax_data_t&, text_buffer_iterator, const text_buffer_iterator)>;

private:
    text_buffer_iterator create_begin_iterator(void) const;
    text_buffer_iterator create_end_iterator(void) const;

public:
    class text_buffer_iterator {
        const std::vector<TextBuffer::Line>* m_lines = nullptr;
        size_t m_line {};
        size_t m_col {};
        size_t m_sz {};
        size_t m_current_line_len {};
        bool issue_newline = false;

        text_buffer_iterator(const std::vector<TextBuffer::Line>* lines);
        static text_buffer_iterator end(const std::vector<TextBuffer::Line>* lines);
        friend text_buffer_iterator TextBuffer::create_begin_iterator(void) const;
        friend text_buffer_iterator TextBuffer::create_end_iterator(void) const;

    public:
        TextBuffer::char_t operator*() const;
        void operator++();
        void operator++(int);
        bool operator==(const text_buffer_iterator& other) const;
        bool operator!=(const text_buffer_iterator& other) const;
        Cursor current_cursor_pos(void) const;
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
    float m_scroll_h = 0.0;
    float m_cursor_dist = 0.0;

    float f_scale_factor;
    float f_line_advance;
    float f_total_height;
    float f_total_width;

    bool m_wrap_lines = false;
    bool m_readonly = false;
    bool m_has_focus = false;
    bool m_draw_cursor = true;
    bool m_do_common_updates = false;

    process_syntax_fn m_syntax_parse_fn = nullptr;
    syntax_data_t m_syntax_data {};

public:
    Color foreground_color = WHITE;
    Color background_color = BLACK;

    TextBuffer() = delete;
    TextBuffer(Font f, Rectangle bounds);

    // generic functions to declutter the code
private:
    void _set_font(std::optional<Font> font = std::nullopt,
        std::optional<int> sz = std::nullopt,
        std::optional<int> spacing = std::nullopt);
    enum class MoveDir {
        VERTICAL,
        HORIZONTAL,
    };
    long _move_cursor(MoveDir dir, long amount, bool with_selection = false);
    enum class ConcatDir {
        FORWARD,
        BACKWARD,
    };
    bool _concat(ConcatDir dir);
    enum class DeleteDir {
        FORWARD,
        BACKWARD,
    };
    void _delete_characters(DeleteDir dir, unsigned long amount);
    void _delete_words(DeleteDir dir, unsigned long amount);
public:
    const Font& get_font() const;
    void set_font(Font font);
    int get_font_size() const;
    void set_font_size(int sz);
    int get_spacing() const;
    void set_spacing(int s);
    float get_width() const;
    void set_width(float w);
    float get_height() const;
    void set_height(float h);
    Rectangle get_bounds() const;
    void set_bounds(Rectangle b);
    Vector2 get_position() const;
    void set_position(Vector2 p);
    line_t get_contents_as_string(void) const;
    void increase_font_size();
    void decrease_font_size();
    bool is_cursor_at_begining(void);
    bool is_cursor_at_end(void);
    void toggle_wrap_lines(void);
    void wrap_lines(bool);
    bool is_wrapping_lines(void) const;
    void toggle_readonly(void);
    bool is_readonly(void) const;
    void toggle_cursor(void);
    bool is_cursor_visible(void) const;
    line_t& current_line(void);
    bool has_focus(void) const;
    size_t get_line_number(void) const;
    const line_t& current_line(void) const;
    const std::optional<Selection>& get_selection(void);
    // if | is the cursor then x is the character
    //
    // x|a
    std::optional<line_t::value_type> get_char_under_cursor(void) const;
    // if | is the cursor then x is the character
    //
    // a|x
    std::optional<line_t::value_type> get_char_after_cursor(void) const;
    size_t get_line_count(void) const;
    long count_chars_to_cursor_in_line(void);
    // moves
    //  returns how many positions the cursor moved (across lines, and counted in bytes)
    long move_cursor_h(long amount);
    long move_cursor_v(long amount);
    long move_cursor_word(long amount, bool with_selection = false);
    long move_cursor_left(long amount = 1, bool with_selection = false);
    long move_cursor_right(long amount = 1, bool with_selection = false);
    void move_cursor_down(long amount = 1, bool with_selection = false);
    void move_cursor_up(long amount = 1, bool with_selection = false);
    // concats
    bool concat_backward(void);
    bool concat_forward(void);
    // deletes
    void delete_characters_back(unsigned long amount = 1);
    void delete_characters_forward(unsigned long amount = 1);
    void delete_words_back(unsigned long amount = 1);
    void delete_words_forward(unsigned long amount = 1);
    void delete_line(size_t line_num);
    void delete_lines(size_t start, size_t end);
    void clear(void);
    // jumps
    void jump_cursor_to_top(bool with_selection = false);
    void jump_cursor_to_bottom(bool with_selection = false);
    void jump_cursor_to_end(bool with_selection = false);
    void jump_cursor_to_start(bool with_selection = false);
    // inserts
    void insert_newline(void);
    void insert_character(char_t c);
    void insert_string(line_t&& str);
    void insert_line(line_t&& str);
    // selection
    void start_selection(void);
    void clear_selection(void);
    void delete_selection(void);
    line_t copy_selection(void);
    line_t cut_selection(void);
    // measures
    float measure_line_till_cursor(void);
    void measure_lines(void);
    void measure_line(Line& line);
    // draws
    void draw(void);
    void draw_vertical_scroll_bar(void);
    // updates
    //  void update_vertical_scroll_bar(Vector2 p);
    void update_buffer_mouse(void);
    void update_buffer(void);
    /// this function ensures that the viewport contains the cursor (the cursor is visible on the screen)
    void update_viewport_to_cursor(void);

    void set_syntax_parser(process_syntax_fn fn);

private:
    std::optional<TextBuffer::Cursor> mouse_as_cursor_position(Vector2 point);
    void clamp_cursor(void);
    void update_scroll_h(void);
    void update_total_height(void);
    void update_font_measurements(void);
    void update_selection(void);
    void update_scroll_v(float v);
    float get_glyph_width(const Font& font, int codepoint) const;
    void update_syntax(void);

public:
    text_buffer_iterator begin(void) const;
    text_buffer_iterator end(void) const;
};
}

#endif
