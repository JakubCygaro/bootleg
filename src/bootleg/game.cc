#include "meu3.h"
#include <bootleg/game.hpp>
#include <bootleg/lua_generics.hpp>
#include <cstdint>
#include <cstdio>
#include <format>
#include <limits>
#include <optional>
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
    unsigned long long len = 0;
    MEU3_BYTES font = meu3_package_get_data_ptr(meu3_pack, path::RESOURCES_FONT.data(), &len, &err);
    if (err != NoError || !font) {
        TraceLog(LOG_ERROR, "Failed to get pointer to font data at `%s`", path::RESOURCES_FONT.data());
    } else {
        Font f = LoadFontFromMemory(".ttf", font, len, 100, NULL, 0);
        if (f.texture.id <= 0) {
            TraceLog(LOG_ERROR, "Failed to load font from resources at `%s`", path::RESOURCES_FONT.data());
        } else {
            SetTextureFilter(f.texture, TEXTURE_FILTER_ANISOTROPIC_8X);
            this->font = f;
        }
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

    auto hw = std::make_unique<boot::HelpWindow>();
    hw->set_bounds(w->get_bounds());

    auto crw = std::make_unique<boot::CreditsWindow>();
    crw->set_bounds(w->get_bounds());

    auto wd = WindowData {
        .win = std::move(lw),
        .name_bounds = {}
    };
    this->windows.push_back(std::move(wd));
    wd.win = std::move(w);
    this->windows.push_back(std::move(wd));
    wd.win = std::move(cw);
    this->windows.push_back(std::move(wd));
    wd.win = std::move(hw);
    this->windows.push_back(std::move(wd));
    wd.win = std::move(crw);
    this->windows.push_back(std::move(wd));

    for (auto& window : this->windows) {
        window.win->init(*this);
    }
    m_current_window = 0;

    update_measurements();

    len = 0;
    if (auto user_conf = (char*)meu3_package_get_data_ptr(meu3_pack, path::USER_CONFIG.data(), &len, &err); user_conf) {
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
    const auto add_color = [=](const std::string_view name, const Color& col) {
        static unsigned char hex[4] = {};
        hex[3] = col.r;
        hex[2] = col.g;
        hex[1] = col.b;
        hex[0] = col.a;
        boot::lua::setglobalv(lua, name.data(), *reinterpret_cast<unsigned int*>(&hex));
    };
    for (const auto& [k, v] : boot::colors::COLORMAP) {
        add_color(k, v);
    }
}
extern "C" {
static int l_color__from_parts(lua_State* L)
{
    int n = lua_gettop(L);
    if (n != 3)
        return 0;
    const bool all_nums = lua_isnumber(L, 1) && lua_isnumber(L, 2) && lua_isnumber(L, 3);
    if (!all_nums)
        return 0;
    constexpr const auto max = std::numeric_limits<decltype(Color::a)>::max();
    Color color = {};
    color.a = max;
    color.r = std::abs(lua_tonumber(L, 1)) * max;
    color.g = std::abs(lua_tonumber(L, 2)) * max;
    color.b = std::abs(lua_tonumber(L, 3)) * max;
    int32_t raw_color = 0;
    int8_t* bytes = reinterpret_cast<int8_t*>(&raw_color);
    bytes[3] = color.r;
    bytes[2] = color.g;
    bytes[1] = color.b;
    bytes[0] = color.a;
    lua_pushinteger(L, raw_color);
    return 1;
}
}
void boot::Game::init_lua_state(void)
{
    if (m_lua_state) {
        lua_close(m_lua_state);
        m_lua_state = NULL;
    }
    m_lua_state = luaL_newstate();
    luaL_openlibs(m_lua_state);
    lua_createtable(m_lua_state, 0, 1);
    lua_pushstring(m_lua_state, "fromRGB");
    lua_pushcfunction(m_lua_state, l_color__from_parts);
    lua_settable(m_lua_state, 1);
    lua_setglobal(m_lua_state, "color");
    lua::setglobalv(m_lua_state, "x", 0);
    lua::setglobalv(m_lua_state, "y", 0);
    lua::setglobalv(m_lua_state, "z", 0);
    lua::setglobalv(m_lua_state, "Color", 0);
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
    level_completed = m_solution.has_value();
    for (int x = 0; x < cube.x; x++) {
        for (int y = 0; y < cube.y; y++) {
            for (int z = 0; z < cube.z; z++) {
                lua::setglobalv(m_lua_state, "X", cube.x);
                lua::setglobalv(m_lua_state, "Y", cube.y);
                lua::setglobalv(m_lua_state, "Z", cube.z);

                lua::setglobalv(m_lua_state, "Color", 0);
                lua::setglobalv(m_lua_state, "x", x);
                lua::setglobalv(m_lua_state, "y", y);
                lua::setglobalv(m_lua_state, "z", z);
                if (luaL_dostring(m_lua_state, source.data())) {
                    auto* err = lua_tostring(m_lua_state, -1);
                    std::printf("pcall failed : %s\n",
                        err);
                    level_completed = false;
                    return err;
                }
                auto c = lua::getglobalv<unsigned int>(m_lua_state, "Color");
                cube.color_data[x][y][z] = boot::decode_color_from_hex(c.value_or(0));
                const auto color_eq = [](const Color& a, const Color& b) -> bool {
                    return a.r == b.r && a.b == b.b && a.g == b.g && a.a == b.a;
                };
                if (m_solution.has_value())
                    level_completed &= color_eq(cube.color_data[x][y][z], m_solution->solution->color_data[x][y][z]);
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
void Game::preload_lua_level(Level& lvl)
{
    if (lvl.ty != Level::Type::Lua)
        return;
    init_lua_state();
    auto data = raw::LevelData {};
    auto error = luaL_loadbuffer(m_lua_state, reinterpret_cast<const char*>(lvl.data_ptr), lvl.data_len, NULL);
    if (error != LUA_OK) {
        TraceLog(LOG_ERROR, "Error while preloading lua levelgen script\n%s", lua_tostring(m_lua_state, -1));
        goto crash_and_burn;
    }
    try {
        lua::voidpcall(m_lua_state, NULL);
    } catch (const std::runtime_error& err) {
        TraceLog(LOG_ERROR, "Error while running lua levelgen script for preload\n%s", err.what());
        goto crash_and_burn;
    }
    data.X = lua::getglobalv<int>(m_lua_state, "X").value_or(-1);
    data.Y = lua::getglobalv<int>(m_lua_state, "Y").value_or(-1);
    data.Z = lua::getglobalv<int>(m_lua_state, "Z").value_or(-1);
    data.name = lua::getglobalv<std::string>(m_lua_state, "Name").value_or("");
    data.desc = lua::getglobalv<std::string>(m_lua_state, "Desc").value_or("");
    lvl.data = data;
crash_and_burn:
    lua_settop(m_lua_state, 0);
    init_lua_state();
}
void Game::load_level(const Level& lvl, std::string name)
{
    m_solution = std::nullopt;
    saved_solution = std::nullopt;
    std::string lvl_name, lvl_desc;
    CubeData *sol {}, *sol_cube {};
    MEU3_Error err = NoError;
    const auto saved_path = std::format("{}/{}", path::USER_SOLUTIONS_DIR, name);
    if (lvl.ty == Level::Type::Lua) {
        init_lua_state();
        auto error = luaL_loadbuffer(m_lua_state, reinterpret_cast<const char*>(lvl.data_ptr), lvl.data_len, NULL);
        if (error != LUA_OK) {
            TraceLog(LOG_ERROR, "Error while loading lua levelgen script\n%s", lua_tostring(m_lua_state, -1));
            goto crash_and_burn;
        }
        try {
            lua::voidpcall(m_lua_state, NULL);
        } catch (const std::runtime_error& err) {
            TraceLog(LOG_ERROR, "Error while running lua levelgen script\n%s", err.what());
            goto crash_and_burn;
        }
        int x {}, y {}, z {};
        const auto get_dim = [&](const char* name, int& out) -> bool {
            auto v = lua::getglobalv<int>(m_lua_state, name);
            if (!v) {
                TraceLog(LOG_ERROR, "Error while running lua levelgen script: variable '%s' was not an integer", name);
                return false;
            }
            out = v.value();
            return true;
        };
        if (!get_dim("X", x))
            goto crash_and_burn;
        if (!get_dim("Y", y))
            goto crash_and_burn;
        if (!get_dim("Z", z))
            goto crash_and_burn;
        lvl_name = lua::getglobalv<std::string>(m_lua_state, "Name").value_or("");
        lvl_desc = lua::getglobalv<std::string>(m_lua_state, "Desc").value_or("");

        m_solution = raw::LevelData {
            .X = x,
            .Y = y,
            .Z = z,
            .solution = CubeData(x, y, z),
            .desc = lvl_desc,
            .name = name,
        };

        sol = &m_solution->solution.value();
        for (int x = 0; x < sol->x; x++) {
            for (int y = 0; y < sol->y; y++) {
                for (int z = 0; z < sol->z; z++) {
                    lua::setglobalv(m_lua_state, "X", sol->x);
                    lua::setglobalv(m_lua_state, "Y", sol->y);
                    lua::setglobalv(m_lua_state, "Z", sol->z);

                    lua::setglobalv(m_lua_state, "Color", 0);
                    lua::setglobalv(m_lua_state, "x", x);
                    lua::setglobalv(m_lua_state, "y", y);
                    lua::setglobalv(m_lua_state, "z", z);
                    try {
                        lua::voidpcall(m_lua_state, "Generate");
                    } catch (const std::runtime_error& err) {
                        TraceLog(LOG_ERROR, "Error while running lua levelgen script\n%s", err.what());
                    }
                    auto c = lua::getglobalv<unsigned int>(m_lua_state, "Color");
                    sol->color_data[x][y][z] = decode_color_from_hex(c.value_or(0));
                }
            }
        }

    } else {
        auto rawlvl = std::string((char*)lvl.data_ptr, lvl.data_len);
        auto lvld = raw::parse_level_data(std::move(rawlvl));
        m_solution = lvld;
    }
    sol_cube = &m_solution->solution.value();
    cube = CubeData(sol_cube->x, sol_cube->y, sol_cube->z);
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
    m_solution = std::nullopt;
    lua_settop(m_lua_state, 0);
    init_lua_state();
    return;
}
void Game::transition_to(std::string_view window_name)
{
    for (size_t i = 0; i < windows.size(); i++) {
        if (window_name == windows[i].win->get_window_name()) {
            m_current_window = i;
            windows[i].win->on_transition(*this);
            return;
        }
    }
}
void Game::save_source_for_current_level(std::string&& solution)
{
    const auto current_lvl_path = std::format("{}/{}", path::USER_SOLUTIONS_DIR, m_current_save_name);
    MEU3_Error err = NoError;
    meu3_package_insert(meu3_pack, current_lvl_path.data(), reinterpret_cast<unsigned char*>(solution.data()), solution.size(), &err);
    if (err != NoError) {
        TraceLog(LOG_ERROR, "Error while trying to save source for level `%s`", m_current_save_name.data());
        return;
    }
    save_game_data();
}
bool Game::save_solution_for_current_level(std::string&& solution)
{
    const auto current_lvl_path = std::format("{}/{}", path::USER_COMPLETED_DIR, m_current_save_name);
    MEU3_Error err = NoError;

    // check if a solution does not already exists and is shorter than the current one
    unsigned long long len = 0;
    if (auto sol = meu3_package_get_data_ptr(meu3_pack, current_lvl_path.data(), &len, &err);
        sol && len <= solution.length()) {
        return false;
    }
    err = NoError;
    meu3_package_insert(meu3_pack, current_lvl_path.data(), reinterpret_cast<unsigned char*>(solution.data()), solution.size(), &err);
    if (err != NoError) {
        TraceLog(LOG_ERROR, "Error while trying to save solution for level `%s`", m_current_save_name.data());
        return false;
    }
    save_game_data();
    return true;
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
    if (auto c = lua::getglobalv<unsigned int>(m_lua_state, "ForeColor"); c) {
        conf.foreground_color = decode_color_from_hex(*c);
    }
    if (auto c = lua::getglobalv<std::string>(m_lua_state, "ForeColor"); c && boot::colors::COLORMAP.contains(*c)) {
        conf.foreground_color = boot::colors::COLORMAP.at(*c);
    }
    if (auto c = lua::getglobalv<unsigned int>(m_lua_state, "BackColor"); c) {
        conf.background_color = decode_color_from_hex(*c);
    }
    if (auto c = lua::getglobalv<std::string>(m_lua_state, "BackColor"); c && boot::colors::COLORMAP.contains(*c)) {
        conf.background_color = boot::colors::COLORMAP.at(*c);
    }
    if (auto b = lua::getglobalv<bool>(m_lua_state, "WrapLines"); b) {
        conf.wrap_lines = *b;
    }
    if (auto i = lua::getglobalv<int>(m_lua_state, "FontSize"); i) {
        conf.font_size = *i;
    }
    if (auto b = lua::getglobalv<bool>(m_lua_state, "Syntax"); b) {
        conf.syntax_highlighting = *b;
    }
    for (auto& [w, b] : windows) {
        w->on_config_reload(conf);
    }
}
const std::optional<raw::LevelData>& Game::get_lvl_data(void)
{
    return this->m_solution;
}
}
