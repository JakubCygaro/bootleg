#include <bootleg/game.hpp>
#include <raylib.h>

constexpr const float WINDOW_BAR_HEIGHT = 1. / 30.;
constexpr const int CUBE_DIMS = 10;

void boot::Game::init()
{
    m_font = GetFontDefault();
    cube = { CUBE_DIMS, CUBE_DIMS, CUBE_DIMS };
    const auto window_bar_h = WINDOW_BAR_HEIGHT * m_dims.y;
    auto w = std::make_unique<boot::EditorWindow>();
    w->set_bounds({
        .x = 0,
        .y = window_bar_h,
        .width = m_dims.x,
        .height = m_dims.y - window_bar_h,
    });
    this->windows.push_back(std::move(w));
    for (auto& window : this->windows) {
        window->init(*this);
    }
}
void boot::Game::deinit()
{
    windows.clear();
}
void boot::Game::update()
{
    bool resized = IsWindowResized();
    if (resized) {
        m_dims = { (float)GetScreenWidth(), (float)GetScreenHeight() };
    }
    const auto window_bar_h = WINDOW_BAR_HEIGHT * m_dims.y;
    for (auto& window : this->windows) {
        if (resized) {
            window->set_bounds({
                .x = 0,
                .y = window_bar_h,
                .width = m_dims.x,
                .height = m_dims.y - window_bar_h,
            });
        }
        window->update(*this);
    }
}
void boot::Game::draw()
{
    BeginDrawing();
    ClearBackground(BLACK);
    for (auto& window : this->windows) {
        window->draw(*this);
    }
    const auto window_bar_h = WINDOW_BAR_HEIGHT * m_dims.y;
    DrawRectangleRec(
        {
            .x = 0,
            .y = 0,
            .width = m_dims.x,
            .height = window_bar_h,
        },
        BLUE);
    EndDrawing();
}
