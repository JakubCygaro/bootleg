#ifndef BOOT_GAME_HPP
#define BOOT_GAME_HPP
#include "bootleg/slider.hpp"
#include <buffer.hpp>
#include <cstddef>
#include <cstring>
#include <format>
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
    inline const std::string RESOURCES_DIR = "game/resources";
    inline const std::string RESOURCES_FONT = std::format("{}/DroidSansMono.ttf", RESOURCES_DIR);
    inline const std::string USER_SOLUTIONS_DIR = "player/levels";
    inline const std::string USER_COMPLETED_DIR = "player/completed";
}
Color decode_color_from_hex(unsigned int hex_color);
using buffer_t = bed::TextBuffer;
void markdown_like_syntax_parser(Color foreground,
    buffer_t::syntax_data_t& syntax,
    buffer_t::text_buffer_iterator tit,
    const buffer_t::text_buffer_iterator end);
void draw_cursor_tooltip(const char* txt, Font font, float font_sz, float spacing, const Rectangle& bounds, Color color);

struct Rotation {
    float angle {};
    float x {};
    float y {};
    float z {};
    constexpr inline explicit Rotation(float angle, float x, float y, float z)
        : angle(angle)
        , x(x)
        , y(y)
        , z(z)
    {
    }
    constexpr inline static Rotation none()
    {
        return Rotation(0, 1.0, 0, 0);
    }
    constexpr inline static Rotation x_axis(float angle)
    {
        return Rotation(angle, 1.0, 0, 0);
    }
    constexpr inline static Rotation y_axis(float angle)
    {
        return Rotation(angle, 0, 1, 0);
    }
    constexpr inline static Rotation z_axis(float angle)
    {
        return Rotation(angle, 0, 1, 0);
    }
};
Vector2 measure_codepoint_3d(int codepoint, Font font, float font_size);
Vector2 measure_text_3d(const char* txt, Font font, float font_size, float spacing);
void draw_text_3d(const char* txt, Font font, Vector3 pos, float font_size,
    float spacing, const Color& color,
    bool backface = false, const Rotation& rotation = Rotation::none());
Vector2 draw_codepoint_3d(int codepoint, Font font, const Vector3& pos,
    float font_size, const Color& color,
    bool backface = false, const Rotation& rotation = Rotation::none());

class Game;

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
        std::optional<CubeData> solution {};
        std::string desc{};
        std::string name{};
    };
    LevelData parse_level_data(std::string&& src, bool skip_data_section = false);
}

struct Level {
    MEU3_BYTES data_ptr {};
    unsigned long long data_len {};
    enum struct Type {
        Lua,
        Raw
    };
    Type ty;
    /// this is partially loaded
    raw::LevelData data;
};
class Game {
public:
    struct WindowData;

private:
    Vector2 m_dims {};
    lua_State* m_lua_state {};
    size_t m_current_window {};
    std::string m_current_save_name {};
    Config m_conf = {};
    std::optional<raw::LevelData> m_solution {};

public:
    Font font;
    CubeData cube {};
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
    void preload_lua_level(Level& lvl);
    Color color_for(int x, int y, int z);
    void transition_to(std::string_view window_name);
    void save_source_for_current_level(std::string&& solution);
    bool save_solution_for_current_level(std::string&& solution);
    void save_game_data(void);
    void reload_configuration(std::string&&);
    const std::optional<raw::LevelData>& get_lvl_data(void);

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
    void handle_clear_completion(Game& game_state);

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
class HelpWindow final : public Window {
    std::unique_ptr<bed::TextBuffer> m_help_buffer = nullptr;

public:
    explicit HelpWindow();
    virtual void init(Game& game_state) override;
    virtual void update(Game& game_state) override;
    virtual void draw(Game& game_state) override;
    virtual const char* get_window_name() override;
    virtual void set_bounds(Rectangle r) override;
    virtual void on_config_reload(const Config& conf) override;
    virtual ~HelpWindow();
};
class CreditsWindow final : public Window {
public:
    explicit CreditsWindow();
    virtual void init(Game& game_state) override;
    virtual void update(Game& game_state) override;
    virtual void draw(Game& game_state) override;
    virtual const char* get_window_name() override;
    virtual ~CreditsWindow();
};
}
#endif
