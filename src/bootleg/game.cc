#include "meu3.h"
#include <bootleg/game.hpp>
#include <cstdio>
#include <format>
#include <optional>
#include <print>
#include <raylib.h>
#include <stdexcept>
#include <string_view>

#ifdef __cplusplus
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}
#endif

constexpr const float WINDOW_BAR_HEIGHT = 1. / 30.;
constexpr const float WINDOW_NAME_FONT_SPACING = 10;
constexpr const int CUBE_DIMS = 10;

void boot::Game::init()
{
    font = GetFontDefault();
    cube = { CUBE_DIMS, CUBE_DIMS, CUBE_DIMS };
    init_lua_state();
    MEU3_Error err = NoError;
    meu3_pack = meu3_load_package(path::GAME_DATA_PATH.data(), &err);
    if (err != NoError) {
        throw std::runtime_error(std::format("Failed to load gamedata meu3 package at `{}`, error code {}", path::GAME_DATA_PATH, (int)err));
    }
    const auto window_bar_h = WINDOW_BAR_HEIGHT * m_dims.y;
    auto w = std::make_unique<boot::EditorWindow>();
    w->set_bounds({
        .x = 0,
        .y = window_bar_h,
        .width = m_dims.x,
        .height = m_dims.y - window_bar_h,
    });
    auto lw = std::make_unique<boot::LevelSelectWindow>();
    lw->set_bounds(w->get_bounds());

    auto cw = std::make_unique<boot::ConfigWindow>();
    cw->set_bounds(w->get_bounds());

    auto wd = WindowData {
        .win = std::move(lw),
        .name_bounds = {}
    };
    this->windows.push_back(std::move(wd));
    wd.win = std::move(w);
    this->windows.push_back(std::move(wd));
    wd.win = std::move(cw);
    this->windows.push_back(std::move(wd));

    for (auto& window : this->windows) {
        window.win->init(*this);
    }
    m_current_window = 0;
    update_measurements();

    unsigned long long len = 0;
    if(auto user_conf = (char*)meu3_package_get_data_ptr(meu3_pack, path::USER_CONFIG.data(), &len, &err); user_conf){
        auto conf = std::string(user_conf, len);
        reload_configuration(std::move(conf));
    } else {
        auto def_conf = (const char*)meu3_package_get_data_ptr(meu3_pack, path::DEF_CONFIG.data(), &len, &err);
        if (err != NoError) {
            TraceLog(LOG_ERROR, "Error while trying to get a ref for default config on game init");
        } else {
            auto conf = std::string(def_conf, len);
            reload_configuration(std::move(conf));
        }
    }
}
static void setup_colors(lua_State* lua)
{
    const auto add_color = [=](const char* name, unsigned int hex) {
        lua_pushinteger(lua, hex);
        lua_setglobal(lua, name);
    };
    add_color("BLANK", boot::colors::CBLANK);
    add_color("RED", boot::colors::CRED);
    add_color("GREEN", boot::colors::CGREEN);
    add_color("BLUE", boot::colors::CBLUE);
    add_color("MAGENTA", boot::colors::CMAGENTA);
    add_color("ORANGE", boot::colors::CORANGE);
    add_color("BLACK", boot::colors::CBLACK);
    add_color("YELLOW", boot::colors::CYELLOW);
}
void boot::Game::init_lua_state(void)
{
    if (m_lua_state) {
        lua_close(m_lua_state);
        m_lua_state = NULL;
    }
    m_lua_state = luaL_newstate();
    luaL_openlibs(m_lua_state);
    lua_pushinteger(m_lua_state, 0);
    lua_setglobal(m_lua_state, "x");
    lua_pushinteger(m_lua_state, 0);
    lua_setglobal(m_lua_state, "y");
    lua_pushinteger(m_lua_state, 0);
    lua_setglobal(m_lua_state, "z");
    lua_pushinteger(m_lua_state, 0);
    lua_setglobal(m_lua_state, "Color");
    setup_colors(m_lua_state);
    lua_settop(m_lua_state, 0);
}
void boot::Game::deinit()
{
    windows.clear();
    meu3_free_package(meu3_pack);
    meu3_pack = nullptr;
}
void boot::Game::update_measurements(void)
{
    const auto window_bar_h = WINDOW_BAR_HEIGHT * m_dims.y;
    const float padding = .02f * this->m_dims.x;
    float offset = padding;
    for (auto& window : this->windows) {
        const auto name = window.win->get_window_name();
        const auto tmpsz = MeasureTextEx(font, name, window_bar_h, WINDOW_NAME_FONT_SPACING);
        window.name_bounds = {
            .x = offset,
            .y = 0,
            .width = tmpsz.x,
            .height = tmpsz.y
        };
        offset += tmpsz.x + padding;
        window.win->set_bounds({
            .x = 0,
            .y = window_bar_h,
            .width = m_dims.x,
            .height = m_dims.y - window_bar_h,
        });
    }
}
void boot::Game::update()
{
    bool resized = IsWindowResized();
    if (resized) {
        m_dims = { (float)GetScreenWidth(), (float)GetScreenHeight() };
        update_measurements();
    }
    if (IsKeyDown(KEY_LEFT_SHIFT) && IsKeyPressed(KEY_TAB)) {
        m_current_window = (m_current_window + 1) % windows.size();
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        const auto mouse = GetMousePosition();
        for (size_t i = 0; i < windows.size(); i++) {
            if (CheckCollisionPointRec(mouse, windows[i].name_bounds)) {
                m_current_window = i;
                break;
            }
        }
    }
    windows[m_current_window].win->update(*this);
}
void boot::Game::draw()
{
    BeginDrawing();
    ClearBackground(BLACK);
    windows[m_current_window].win->draw(*this);
    const auto window_bar_h = WINDOW_BAR_HEIGHT * m_dims.y;
    DrawRectangleRec(
        {
            .x = 0,
            .y = 0,
            .width = m_dims.x,
            .height = window_bar_h,
        },
        BLUE);
    for (size_t i = 0; i < windows.size(); i++) {
        auto name = windows[i].win->get_window_name();
        auto name_bounds = windows[i].name_bounds;
        if (i == m_current_window) {
            auto back = name_bounds;
            back.x -= m_dims.x * 0.01;
            back.width += m_dims.x * 0.02;
            DrawRectangleRec(back, WHITE);
            DrawTextEx(font, name, { name_bounds.x, name_bounds.y }, window_bar_h, WINDOW_NAME_FONT_SPACING, BLUE);
        } else {
            DrawTextEx(font, name, { name_bounds.x, name_bounds.y }, window_bar_h, WINDOW_NAME_FONT_SPACING, WHITE);
        }
    }
    EndDrawing();
}
Color boot::decode_color_from_hex(unsigned int hex_color)
{
    Color ret = {};
    ret.a |= hex_color;
    hex_color >>= 8;
    ret.b |= hex_color;
    hex_color >>= 8;
    ret.g |= hex_color;
    hex_color >>= 8;
    ret.r |= hex_color;
    return ret;
}
std::optional<std::string> boot::Game::load_source(const std::string& source)
{
    for (int x = 0; x < cube.x; x++) {
        for (int y = 0; y < cube.y; y++) {
            for (int z = 0; z < cube.z; z++) {
                lua_pushinteger(m_lua_state, 0);
                lua_setglobal(m_lua_state, "Color");
                lua_pushinteger(m_lua_state, x);
                lua_setglobal(m_lua_state, "x");
                lua_pushinteger(m_lua_state, y);
                lua_setglobal(m_lua_state, "y");
                lua_pushinteger(m_lua_state, z);
                lua_setglobal(m_lua_state, "z");
                if (luaL_dostring(m_lua_state, source.data())) {
                    auto* err = lua_tostring(m_lua_state, -1);
                    std::printf("pcall failed : %s\n",
                        err);
                    return err;
                }
                lua_getglobal(m_lua_state, "Color");
                unsigned int c = lua_tointeger(m_lua_state, -1);
                lua_settop(m_lua_state, 0);
                cube.color_data[x][y][z] = boot::decode_color_from_hex(c);
            }
        }
    }
    return std::nullopt;
}
Color boot::Game::color_for(int x, int y, int z)
{
    return this->cube.color_data[x][y][z];
}
namespace boot {
void Game::load_level(const Level& lvl, std::string name)
{
    solution = std::nullopt;
    saved_solution = std::nullopt;
    MEU3_Error err = NoError;
    const auto saved_path = std::format("{}/{}", path::USER_SOLUTIONS_DIR, name);
    if (lvl.ty == Level::Type::Lua) {
        // std::printf("%.*s\n", (int)lvl.data_len, (char*)lvl.data_ptr);
        init_lua_state();
        auto error = luaL_loadbuffer(m_lua_state, reinterpret_cast<const char*>(lvl.data_ptr), lvl.data_len, "levelgen")
            || lua_pcall(m_lua_state, 0, 0, 0);
        // if (error) {
        //     TraceLog(LOG_ERROR, "Error while running lua levelgen script\n%s", lua_tostring(m_lua_state, -1));
        //     goto crash_and_burn;
        // }
        int x {}, y {}, z {};
        lua_getglobal(m_lua_state, "X");
        if (!lua_isinteger(m_lua_state, -1)) {
            TraceLog(LOG_ERROR, "Error while running lua levelgen script: X was not an integer (%s)", lua_typename(m_lua_state, -1));
            goto crash_and_burn;
        }
        x = lua_tointeger(m_lua_state, -1);
        lua_getglobal(m_lua_state, "Y");
        if (!lua_isinteger(m_lua_state, -1)) {
            TraceLog(LOG_ERROR, "Error while running lua levelgen script: Y was not an integer");
            goto crash_and_burn;
        }
        y = lua_tointeger(m_lua_state, -1);
        lua_getglobal(m_lua_state, "Z");
        if (!lua_isinteger(m_lua_state, -1)) {
            TraceLog(LOG_ERROR, "Error while running lua levelgen script: Z was not an integer");
            goto crash_and_burn;
        }
        z = lua_tointeger(m_lua_state, -1);
        lua_settop(m_lua_state, 0);

        TraceLog(LOG_DEBUG, "X = %d Y = %d Z = %d", x, y, z);

        solution = CubeData(x, y, z);
        for (int x = 0; x < solution->x; x++) {
            for (int y = 0; y < solution->y; y++) {
                for (int z = 0; z < solution->z; z++) {
                    lua_pushinteger(m_lua_state, solution->x);
                    lua_setglobal(m_lua_state, "X");
                    lua_pushinteger(m_lua_state, solution->y);
                    lua_setglobal(m_lua_state, "Y");
                    lua_pushinteger(m_lua_state, solution->z);
                    lua_setglobal(m_lua_state, "Z");

                    lua_pushinteger(m_lua_state, 0);
                    lua_setglobal(m_lua_state, "Color");
                    lua_pushinteger(m_lua_state, x);
                    lua_setglobal(m_lua_state, "x");
                    lua_pushinteger(m_lua_state, y);
                    lua_setglobal(m_lua_state, "y");
                    lua_pushinteger(m_lua_state, z);
                    lua_setglobal(m_lua_state, "z");

                    lua_getglobal(m_lua_state, "Generate");
                    if (!lua_isfunction(m_lua_state, -1)) {
                        TraceLog(LOG_ERROR, "Error while running lua levelgen script: Generate function missing");
                        goto crash_and_burn;
                    }
                    if (lua_pcall(m_lua_state, 0, 0, 0)) {
                        TraceLog(LOG_ERROR, "Error while running lua levelgen script\n%s", lua_tostring(m_lua_state, -1));
                        goto crash_and_burn;
                    }

                    lua_getglobal(m_lua_state, "Color");
                    unsigned int c = lua_tointeger(m_lua_state, -1);
                    lua_settop(m_lua_state, 0);
                    solution->color_data[x][y][z] = decode_color_from_hex(c);
                }
            }
        }

    } else {
        auto rawlvl = std::string((char*)lvl.data_ptr, lvl.data_len);
        auto lvld = raw::parse_level_data(std::move(rawlvl));
        solution = lvld.solution;
    }
    cube = CubeData(solution->x, solution->y, solution->z);
    m_current_save_name = name;
    if (meu3_package_has(meu3_pack, saved_path.data(), &err)) {
        auto len = 0ull;
        auto ptr = (char*)meu3_package_get_data_ptr(meu3_pack, saved_path.data(), &len, &err);
        if (err != NoError) {
            TraceLog(LOG_ERROR, "Error while trying to load a saved solution for level `%s`", saved_path.data());
        } else {
            saved_solution = std::string(ptr, len);
        }
    }
    return;
crash_and_burn:
    solution = std::nullopt;
    lua_settop(m_lua_state, 0);
    init_lua_state();
    return;
}
void Game::transition_to(std::string_view window_name)
{
    for (size_t i = 0; i < windows.size(); i++) {
        if (window_name == windows[i].win->get_window_name()) {
            m_current_window = i;
            return;
        }
    }
}
void Game::save_solution_for_current_level(std::string&& solution)
{
    const auto current_lvl_path = std::format("{}/{}", path::USER_SOLUTIONS_DIR, m_current_save_name);
    MEU3_Error err = NoError;
    meu3_package_insert(meu3_pack, current_lvl_path.data(), reinterpret_cast<unsigned char*>(solution.data()), solution.size(), &err);
    if (err != NoError) {
        TraceLog(LOG_ERROR, "Error while trying to save solution for level `%s`", m_current_save_name.data());
        return;
    }
    save_game_data();
}
void Game::save_game_data(void)
{
    MEU3_Error err = NoError;
    meu3_write_package(path::GAME_DATA_PATH.data(), meu3_pack, &err);
    if (err != NoError) {
        TraceLog(LOG_ERROR, "Error while trying to save game data ");
        return;
    }
}
void Game::reload_configuration(std::string&& config_source)
{
    Config& conf = m_conf;
    init_lua_state();
    luaL_dostring(m_lua_state, config_source.data());
    lua_getglobal(m_lua_state, "ForeColor");
    conf.foreground_color = decode_color_from_hex(lua_tointeger(m_lua_state, -1));
    lua_getglobal(m_lua_state, "BackColor");
    conf.background_color = decode_color_from_hex(lua_tointeger(m_lua_state, -1));
    lua_getglobal(m_lua_state, "WrapLines");
    conf.wrap_lines = lua_toboolean(m_lua_state, -1);
    lua_getglobal(m_lua_state, "FontSize");
    conf.font_size = lua_tointeger(m_lua_state, -1);
    lua_settop(m_lua_state, 0);
    for (auto& [w, b] : windows) {
        w->on_config_reload(conf);
    }
}
}
