#include <bootleg/game.hpp>
#include <cstdio>
#include <print>
#include <raylib.h>

#ifdef __cplusplus
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}
#endif

constexpr const float WINDOW_BAR_HEIGHT = 1. / 30.;
constexpr const int CUBE_DIMS = 10;

void boot::Game::init()
{
    m_font = GetFontDefault();
    cube = { CUBE_DIMS, CUBE_DIMS, CUBE_DIMS };
    init_lua_state();
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
static void setup_colors(lua_State* lua) {
    const auto add_color = [=](const char* name, unsigned int hex){
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
    const float padding = 2.0;
    float offset = 0.0 + padding;
    for (auto& window : this->windows) {
        auto name = window->get_window_name();
        auto tmpsz = MeasureTextEx(m_font, name, window_bar_h, 10);
        DrawTextEx(m_font, name, { 0 + offset, 0 }, window_bar_h, 10, WHITE);
        offset += tmpsz.x + padding;
    }
    EndDrawing();
}
static Color decode_color_from_hex(unsigned int hex_color){
    Color ret = { };
    ret.a |= hex_color;
    hex_color >>= 8;
    ret.b |= hex_color;
    hex_color >>= 8;
    ret.g |= hex_color;
    hex_color >>= 8;
    ret.r |= hex_color;
    std::println("{:x}", hex_color);
    return ret;
}
void boot::Game::load_source(const std::string& source)
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
                if(luaL_dostring(m_lua_state, source.data())){
                    std::printf("pcall failed : %s\n",
                            lua_tostring(m_lua_state, -1));
                    return;
                }
                lua_getglobal(m_lua_state, "Color");
                unsigned int c = lua_tointeger(m_lua_state, -1);
                lua_settop(m_lua_state, 0);
                cube.color_data[x][y][z] = decode_color_from_hex(c);
            }
        }
    }
}
Color boot::Game::color_for(int x, int y, int z)
{
    return this->cube.color_data[x][y][z];
}
