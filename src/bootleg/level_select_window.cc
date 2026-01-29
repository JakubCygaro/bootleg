#include "bootleg/game.hpp"
#include <format>
#include <meu3.h>
#include <print>
#include <raylib.h>
namespace boot {
LevelSelectWindow::LevelSelectWindow()
{
}
void LevelSelectWindow::init(Game& game_state)
{
    const auto base_path = "game/levels/";
    for (auto i = 1;; i++) {
        const auto lvl_path = std::format("{}lvl{}.lua", base_path, i);
        TraceLog(LOG_DEBUG, "Checking level `%s`", lvl_path.data());
        MEU3_Error err = NoError;
        auto has = meu3_package_has(game_state.meu3_pack, lvl_path.data(), &err);
        if (err != NoError) {
            TraceLog(LOG_ERROR, "Error while loading level `%s`, error code: %d", lvl_path.data(), err);
            continue;
        }
        if (has) {
            TraceLog(LOG_DEBUG, "Found level `%s`", lvl_path.data());
            Level lvl = {};
            lvl.data_ptr = meu3_package_get_data_ptr(game_state.meu3_pack, lvl_path.data(), &lvl.data_len, &err);
            lvl.ty = Level::Type::Lua;
            game_state.levels.push_back(std::move(lvl));
        } else {
            break;
        }
    }
}
void LevelSelectWindow::update(Game& game_state)
{
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        return;
    const auto mouse = GetMousePosition();
    Vector2 pos = { m_bounds.x + (m_bounds.width / 2), m_bounds.y };
    auto lvl_it = game_state.levels.begin();
    const std::string name = std::format("level-00");
    auto sz = MeasureTextEx(game_state.font, name.data(), 24, 12);
    pos.x -= sz.x / 2.;
    for (; lvl_it != game_state.levels.end(); lvl_it++) {
        if (CheckCollisionPointRec(mouse, { pos.x, pos.y, sz.x, sz.y })) {
            TraceLog(LOG_DEBUG, "attempting to load level");
            game_state.load_level_solution(*lvl_it);
            return;
        }
        pos.y += sz.y + 2;
    }
}
void LevelSelectWindow::draw(Game& game_state)
{
    Vector2 pos = { m_bounds.x + (m_bounds.width / 2), m_bounds.y };
    auto lvl_it = game_state.levels.begin();
    for (auto i = 1; lvl_it != game_state.levels.end(); i++, lvl_it++) {
        const std::string name = std::format("level-{}", i);
        auto sz = MeasureTextEx(game_state.font, name.data(), 24, 12);
        DrawText(name.data(), static_cast<int>(pos.x - sz.x / 2), static_cast<int>(pos.y), 24, WHITE);
        pos.y += sz.y + 2;
    }
}
const char* LevelSelectWindow::get_window_name()
{
    static std::string name = "levels";
    return name.data();
}
LevelSelectWindow::~LevelSelectWindow()
{
}
}
