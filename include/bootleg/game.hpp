#ifndef BOOT_GAME_HPP
#define BOOT_GAME_HPP
#include "bootleg/slider.hpp"
#include <buffer.hpp>
#include <cstddef>
#include <cstring>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#ifdef __cplusplus
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}
#endif
#include "meu3.h"
#include <memory>
#include <raylib.h>
#include <vector>

#ifndef STRINGIFY
#define STRINGIFY(TOKEN) #TOKEN##sv
#endif
namespace boot {
template <class Out, class In>
    requires std::is_constructible_v<Out, In>
Out into(const In& in)
{
    Out out = Out(in);
    return out;
}
namespace colors {
    using namespace std::string_view_literals;
    struct case_insensitive_eq : public std::equal_to<std::string_view> {
        constexpr bool operator()(const std::string_view& lhs, const std::string_view& rhs) const
        {
            const auto pred = [](char a, char b) {
                return std::tolower(a) == std::tolower(b);
            };
            return std::ranges::equal(lhs, rhs, pred);
        }
    };
    inline static const std::unordered_map<
        std::string_view,
        Color,
        std::hash<std::string_view>,
        case_insensitive_eq>
        COLORMAP = { {
            { STRINGIFY(BLANK), BLANK },
            { STRINGIFY(RED), RED },
            { STRINGIFY(GREEN), GREEN },
            { STRINGIFY(BLUE), BLUE },
            { STRINGIFY(MAGENTA), MAGENTA },
            { STRINGIFY(ORANGE), ORANGE },
            { STRINGIFY(YELLOW), YELLOW },
            { STRINGIFY(PINK), PINK },
            { STRINGIFY(BLACK), BLACK },
            { STRINGIFY(WHITE), WHITE },
            { STRINGIFY(GRAY), GRAY },
            { STRINGIFY(BROWN), BROWN },
        } };
    inline constexpr const auto X_AXIS = RED;
    inline constexpr const auto Y_AXIS = GREEN;
    inline constexpr const auto Z_AXIS = BLUE;
}
namespace path {
    inline const std::string GAME_DATA_PATH = "gamedata.m3pkg";
    inline const std::string DEF_CONFIG = "game/config/config.lua";
    inline const std::string USER_CONFIG = "player/config.lua";
    inline const std::string LEVELS_DIR = "game/levels";
    inline const std::string USER_SOLUTIONS_DIR = "player/levels";
    inline const std::string USER_COMPLETED_DIR = "player/completed";
}
Color decode_color_from_hex(unsigned int hex_color);

class Game;

struct Level {
    MEU3_BYTES data_ptr {};
    unsigned long long data_len {};
    enum struct Type {
        Lua,
        Raw
    };
    Type ty;
};
struct Config {
    Color foreground_color = WHITE;
    Color background_color = BLACK;
    bool wrap_lines = false;
    int font_size = 40;
    bool syntax_highlighting = true;
};
struct Window {
protected:
    Rectangle m_bounds {};

public:
    virtual void init(Game& game_state) = 0;
    virtual void update(Game& game_state) = 0;
    virtual void draw(Game& game_state) = 0;
    virtual const char* get_window_name() = 0;
    inline virtual void set_bounds(Rectangle r)
    {
        this->m_bounds = r;
    };
    inline virtual const Rectangle& get_bounds()
    {
        return m_bounds;
    }
    inline virtual void on_config_reload(const Config& conf) { };
    inline virtual void on_transition(Game& game_state) { };
    inline virtual ~Window() { };
};

struct CubeData {
    int x {}, y {}, z {};
    std::vector<std::vector<std::vector<Color>>> color_data {};
    inline CubeData(int x, int y, int z)
        : x(x)
        , y(y)
        , z(z)
        , color_data(x, std::vector<std::vector<Color>>(y, std::vector<Color>(z)))
    {
    }
    inline CubeData()
        : x(0)
        , y(0)
        , z(0)
        , color_data(0)
    {
    }
};
namespace raw {
    struct LevelData {
        int X {}, Y {}, Z {};
        CubeData solution {};
    };
    LevelData parse_level_data(std::string&& src);
}

class Game {
public:
    struct WindowData;

private:
    Vector2 m_dims {};
    lua_State* m_lua_state {};
    size_t m_current_window {};
    std::string m_current_save_name {};
    Config m_conf = {};

public:
    Font font;
    CubeData cube {};
    std::optional<CubeData> solution {};
    MEU3_PACKAGE* meu3_pack {};
    std::vector<Level> levels {};
    std::optional<std::string> saved_solution {};
    bool level_completed = false;
    inline Game(float w, float h)
        : m_dims(w, h)
    {
    }
    struct WindowData {
        std::unique_ptr<Window> win {};
        Rectangle name_bounds {};
    };
    // windows? I'm gonna puke
    std::vector<WindowData> windows {};
    void init();
    void deinit();
    void update();
    void draw();
    void update_measurements(void);
    std::optional<std::string> load_source(const std::string&);
    void load_level(const Level& lvl, std::string name);
    Color color_for(int x, int y, int z);
    void transition_to(std::string_view window_name);
    void save_source_for_current_level(std::string&& solution);
    bool save_solution_for_current_level(std::string&& solution);
    void save_game_data(void);
    void reload_configuration(std::string&&);

private:
    void init_lua_state(void);
};
class EditorWindow final : public Window {
    std::unique_ptr<bed::TextBuffer> m_text_buffer;
    std::unique_ptr<bed::TextBuffer> m_output_buffer;
    Camera3D m_camera = {};
    RenderTexture2D m_render_tex = {};
    Vector2 m_render_tex_dims = { 800, 800 };
    Rectangle m_cube_bounds = {};
    Slider m_slider = {};
public:
    explicit EditorWindow();
    virtual void init(Game& game_state) override;
    virtual void update(Game& game_state) override;
    virtual void draw(Game& game_state) override;
    virtual const char* get_window_name() override;
    virtual void set_bounds(Rectangle r) override;
    virtual ~EditorWindow();
    virtual void on_config_reload(const Config& conf) override;
    virtual void on_transition(Game& game_state) override;

private:
    void update_bounds(void);
};
class LevelSelectWindow final : public Window {
    std::unique_ptr<bed::TextBuffer> m_lvl_text_buffer = nullptr;
    std::unique_ptr<bed::TextBuffer> m_lvl_menu_buffer = nullptr;
    std::unordered_map<std::string, size_t> m_lvl_name_idx_map = {};

    int m_current_level = -1;
    std::unordered_map<std::string_view, std::function<void(Game&)>> m_menu_handlers {};

private:
    void handle_level_load(Game& game_state);
    void handle_clear_solution(Game& game_state);
    void handle_load_completion(Game& game_state);

public:
    explicit LevelSelectWindow();
    virtual void init(Game& game_state) override;
    virtual void update(Game& game_state) override;
    virtual void draw(Game& game_state) override;
    virtual const char* get_window_name() override;
    virtual void set_bounds(Rectangle r) override;
    virtual void on_config_reload(const Config& conf) override;
    virtual ~LevelSelectWindow();
};
class ConfigWindow final : public Window {
    std::unique_ptr<bed::TextBuffer> m_config_text_buffer = nullptr;

public:
    explicit ConfigWindow();
    virtual void init(Game& game_state) override;
    virtual void update(Game& game_state) override;
    virtual void draw(Game& game_state) override;
    virtual const char* get_window_name() override;
    virtual void set_bounds(Rectangle r) override;
    virtual void on_config_reload(const Config& conf) override;
    virtual ~ConfigWindow();
};
}
#endif
