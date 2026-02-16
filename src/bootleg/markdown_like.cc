#include "bootleg/game.hpp"
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
void boot::markdown_like_syntax_parser(Color foreground,
    buffer_t::syntax_data_t& syntax,
    buffer_t::text_buffer_iterator tit,
    const buffer_t::text_buffer_iterator end){
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
