#include "buffer.hpp"
#include <bootleg/game.hpp>
#include <functional>
#include <memory>
#include <print>
#include <raylib.h>
#include <string>

static const std::string HELP_TEXT = R"#(
# Welcome to Bootleg, a bad clone of a game I once saw.

The point of the game is to write Lua code that recreates specified shapes
in a 3-dimensional grid.

# Levels
You select a level in the levels tab, first you place the cursor on the level
you want to load into the editor (the left text buffer). Then you press enter
and a choice menu opens up on the right buffer. You will be presented with
several options like:
 - [LOAD LEVEL]: which loads the level into the editor and opens the editor tab
 - [CLEAR SAVED SOLUTION]: which clears the solution you have saved for this level
 - [LOAD COMPLETION]: which loads the Lua source code that solved the level in least bytes (saved automatically)
 - [CLEAR COMPLETION]: which clears the Lua source code that solved the level in least bytes

# Editor
You can write Lua source in the left buffer, the upper right window lets you see the current
task and what your solution looks like. The lower right buffer is for Lua errors and level completion
messages.

## Editor text buffer
The editor buffer works like a regular text buffer except it has additional special Vim-like movement shortcuts:
(C stands for Control)
 - <C-k> move up,
 - <C-j> move down,
 - <C-h> move left,
 - <C-l> move right,
 - <C-w> jump one word forward,
 - <C-b> jump one word backward,
 - <C-a> jump to the start of the line,
 - <C-e> jump to the end of the line,
 - <C-t> jump to the top of the buffer,
 - <C-g> jump to the bottom of the buffer,

When you want to execute the Lua code you press <Shift-Enter>
When you want to save the Lua code you press <Control-s>

## Editor solution window
You can drag the camera by pressing and holding the right mouse button, you can also zoom with the mouse wheel while at it.

You can vertically slice the solution with the yellow scroll bar on the left,

## Configuration tab
This is the buffer you use to configure the game. Stuff is explained there.
)#";
using buffer_t = bed::TextBuffer;
static void process_syntax(Color foreground,
    buffer_t::syntax_data_t& syntax,
    buffer_t::text_buffer_iterator tit,
    const buffer_t::text_buffer_iterator end);

static Rectangle bounds_for_help_tbuf(const Rectangle& w_bounds)
{
    constexpr const float buf_margin = 0.05f;
    const float margin_w = w_bounds.width * buf_margin;
    const float margin_h = w_bounds.height * buf_margin;
    return Rectangle {
        .x = w_bounds.x + margin_w / 2,
        .y = w_bounds.y + margin_h / 2,
        .width = (w_bounds.width) - (margin_w),
        .height = (w_bounds.height) - (margin_h)
    };
}
namespace boot {
HelpWindow::HelpWindow()
{
}
void HelpWindow::init(Game& game_state)
{
    m_help_buffer = std::make_unique<bed::TextBuffer>(game_state.font, Rectangle {});
    auto helptext = std::string(HELP_TEXT);
    m_help_buffer->insert_string(std::move(helptext));
    m_help_buffer->toggle_readonly();
    m_help_buffer->toggle_wrap_lines();
    m_help_buffer->toggle_cursor();
}
void HelpWindow::update(Game& game_state)
{
    m_help_buffer->update_buffer();
}
void HelpWindow::draw(Game& game_state)
{
    DrawRectangleGradientEx(m_bounds, PINK, WHITE, PINK, WHITE);
    m_help_buffer->draw();
}
const char* HelpWindow::get_window_name()
{
    const static std::string name = "help";
    return name.data();
}
void HelpWindow::set_bounds(Rectangle r)
{
    Window::set_bounds(r);
    if (m_help_buffer)
        m_help_buffer->set_bounds(bounds_for_help_tbuf(r));
}
void HelpWindow::on_config_reload(const Config& conf)
{
    if (m_help_buffer) {
        m_help_buffer->foreground_color = conf.foreground_color;
        m_help_buffer->background_color = conf.background_color;
        using namespace std::placeholders;
        m_help_buffer->set_syntax_parser(std::bind(process_syntax, conf.foreground_color, _1, _2, _3));
    }
}
HelpWindow::~HelpWindow()
{
}
}
namespace tokens {
static const Color DIGIT = boot::decode_color_from_hex(0xB4CCA1FF);
static const Color LIST_ELEMENT = boot::decode_color_from_hex(0xc2663aFF);
static const Color KEYWORD_PURPLE = boot::decode_color_from_hex(0xC185BCFF);
static const Color HEADER_1 = boot::decode_color_from_hex(0x4194D4FF);
static const Color HEADER_2 = LIST_ELEMENT;
static const Color HEADER_3 = boot::decode_color_from_hex(0xdbdba9);
static const Color HEADER_4 = boot::decode_color_from_hex(0x4EC37FFF);
static const Color HEADER_5 = HEADER_1;
static const Color HEADER_6 = boot::decode_color_from_hex(0xC185BCFF);
static const Color BRACKETS = HEADER_4;
}
static void process_syntax(Color foreground,
    buffer_t::syntax_data_t& syntax,
    buffer_t::text_buffer_iterator tit,
    const buffer_t::text_buffer_iterator end)
{
    buffer_t::Cursor pos = tit.current_cursor_pos();
    for (; tit != end;) {
        pos = tit.current_cursor_pos();
        buffer_t::char_t c = *tit;
        char ch = 0;
        switch (c) {
        case '#': {
            int hash_count = 1;
            bool prev_was_hash = true;
            tit++;
            if (tit == end)
                break;
            ch = *tit;
            if (ch != ' ' && ch != '#')
                break;
            syntax[pos] = tokens::HEADER_1;
            for (; tit != end; tit++) {
                const auto ch = *tit;
                if (ch == '#' && prev_was_hash) {
                    hash_count++;
                    if (hash_count == 2) {
                        syntax[pos] = tokens::HEADER_2;
                    } else if (hash_count == 3) {
                        syntax[pos] = tokens::HEADER_3;
                    } else if (hash_count == 4) {
                        syntax[pos] = tokens::HEADER_4;
                    } else if (hash_count == 5) {
                        syntax[pos] = tokens::HEADER_5;
                    } else if (hash_count == 6) {
                        syntax[pos] = tokens::HEADER_6;
                    } else {
                        syntax[pos] = foreground;
                    }
                } else {
                    prev_was_hash = false;
                }
                if (ch == '\n') {
                    goto skip_pos_increment;
                }
            }

        } break;
        case ' ':
            tit++;
            if (tit == end)
                break;
            ch = *tit;
            if (ch != '-' && ch != '*' && ch != '+') {
                syntax[pos] = foreground;
                goto skip_pos_increment;
            };
            syntax[pos] = tokens::LIST_ELEMENT;
            break;
        case '\n':
            syntax[pos] = foreground;
            break;
        case '[': {
            int closed = 0;
            for (; tit != end; tit++) {
                const auto ch = *tit;
                if (ch == '[')
                    closed++;
                else if (ch == ']')
                    closed--;
                else if (ch == '\n')
                    goto skip_pos_increment;
                if (closed == 0) {
                    syntax[pos] = tokens::BRACKETS;
                    tit++;
                    pos = tit.current_cursor_pos();
                    syntax[pos] = foreground;
                    break;
                }
            }
        } break;
        case '<': {
            int closed = 0;
            for (; tit != end; tit++) {
                const auto ch = *tit;
                if (ch == '<')
                    closed++;
                else if (ch == '>')
                    closed--;
                else if (ch == '\n')
                    goto skip_pos_increment;
                if (closed == 0) {
                    syntax[pos] = tokens::HEADER_6;
                    tit++;
                    pos = tit.current_cursor_pos();
                    syntax[pos] = foreground;
                    break;
                }
            }
        } break;
        default:
            syntax[pos] = foreground;
            break;
        }
        tit++;
    skip_pos_increment:
    }
}
