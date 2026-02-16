#include "bootleg/game.hpp"
#include "version.h"
#include <raylib.h>
#include <raymath.h>
namespace boot {
CreditsWindow::CreditsWindow()
{
}
void CreditsWindow::init(Game& game_state) { }
void CreditsWindow::update(Game& game_state) { }
void CreditsWindow::draw(Game& game_state)
{
    const auto dims = Vector2 { m_bounds.x + m_bounds.width, m_bounds.y + m_bounds.height };
    const auto center = Vector2Scale(dims, 0.5);
    constexpr auto Bootleg = "Bootleg";
    constexpr auto Agameby = "A game by Adam Papieros";
    constexpr auto version = "version " VERSION_MAJOR "." VERSION_MINOR "." VERSION_PATCH;

    const auto v_sz = MeasureTextEx(game_state.font, Agameby, 30, 10);

    DrawRectangleGradientEx(m_bounds, SKYBLUE, GOLD, SKYBLUE, GOLD);

    const auto back_dims = Vector2 {
        .x = v_sz.x * 1.02f,
        .y = dims.y / 2,
    };
    const auto back_pos = Vector2Subtract(center, Vector2Scale(back_dims, 0.5));
    DrawRectangleRounded({back_pos.x, back_pos.y, back_dims.x, back_dims.y}, 0.3, 5, SKYBLUE);

    auto sz = MeasureTextEx(game_state.font, Bootleg, 60, 10);
    auto pos = center;
    pos.x -= sz.x / 2;
    DrawTextEx(game_state.font, Bootleg, pos, 60, 10, BLACK);

    pos = center;
    pos.y += (sz.y * 1.05);
    pos.x -= v_sz.x / 2;
    DrawTextEx(game_state.font, Agameby, pos, 30, 10, WHITE);

    pos.x += v_sz.x / 2;
    pos.y += (sz.y * 1.05);
    sz = MeasureTextEx(game_state.font, version, 20, 10);
    pos.x -= sz.x / 2;
    DrawTextEx(game_state.font, version, pos, 20, 10, WHITE);
}
const char* CreditsWindow::get_window_name()
{
    static const std::string name = "credits";
    return name.data();
}
CreditsWindow::~CreditsWindow() { }
}
