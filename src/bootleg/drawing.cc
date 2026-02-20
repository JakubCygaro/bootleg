#include "bootleg/game.hpp"
#include <raylib.h>
namespace boot {
void draw_cursor_tooltip(const char* txt, Font font, float font_sz, float spacing, const Rectangle& bounds, Color color)
{
    const auto mouse_pos = GetMousePosition();
    const auto size = MeasureTextEx(font, txt, font_sz, spacing);
    const Vector2 text_pos = {
        .x = std::clamp(mouse_pos.x + size.x, bounds.x + size.x, bounds.x + bounds.width) - size.x,
        .y = std::clamp(mouse_pos.y, bounds.y + size.y, bounds.y + bounds.height) - size.y,
    };
    const Vector2 back = {
        .x = size.x * 1.08f,
        .y = size.y * 1.08f,
    };
    const Vector2 back_adjust = {
        .x = (back.x - size.x) / 2,
        .y = (back.y - size.y) / 2,
    };
    const Rectangle back_r = {
        .x = text_pos.x - back_adjust.x,
        .y = text_pos.y - back_adjust.y,
        .width = back.x,
        .height = back.y,
    };
    const Color back_c = boot::decode_color_from_hex(0xd9d9e1A0);
    DrawRectangleRounded(back_r, 0.5, 10, back_c);
    DrawTextEx(font, txt, text_pos, font_sz, spacing, color);
}
}
