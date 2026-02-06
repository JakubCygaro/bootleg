#ifndef BOOT_GAME_HPP
#define BOOT_GAME_HPP
#include <buffer.hpp>
#include <cstddef>
#include <cstring>
#include <format>
#include <string_view>
#include <tuple>
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
}
namespace path {
    inline const std::string GAME_DATA_PATH = "gamedata.m3pkg";
    inline const std::string DEF_CONFIG = "game/config/config.lua";
    inline const std::string USER_CONFIG = "player/config.lua";
    inline const std::string LEVELS_DIR = "game/levels";
    inline const std::string USER_SOLUTIONS_DIR = "player/levels";
}
Color decode_color_from_hex(unsigned int hex_color);

namespace lua {

    namespace {
        template <typename T>
        void genericpush(lua_State* L, const T& val)
        {
            if constexpr (std::is_same_v<T, bool>) {
                lua_pushboolean(L, val);
            } else if constexpr (std::is_integral_v<T>) {
                lua_pushinteger(L, val);
            } else if constexpr (std::is_floating_point_v<T>) {
                lua_pushnumber(L, val);
            } else if constexpr (std::is_same_v<T, const char*>) {
                lua_pushstring(L, val);
            } else {
                static_assert(false, "Unsupported type T");
            }
        }
        template <typename T>
        inline std::optional<T> genericget(lua_State* L)
        {
            if (lua_isnil(L, -1))
                return std::nullopt;
            bool check = false;
            if constexpr (std::is_same_v<T, bool>) {
                check = lua_isboolean(L, -1);
            } else if constexpr (std::is_integral_v<T>) {
                check = lua_isinteger(L, -1);
            } else if constexpr (std::is_floating_point_v<T>) {
                check = lua_isnumber(L, -1);
            } else if constexpr (std::is_same_v<T, const char*> || std::is_same_v<T, std::string>) {
                check = lua_isstring(L, -1);
            } else {
                static_assert(false, "Unsupported type T");
            }
            if (!check) {
                lua_settop(L, 0);
                return std::nullopt;
            }
            std::optional<T> ret;
            if constexpr (std::is_same_v<T, bool>) {
                ret = lua_toboolean(L, -1);
            } else if constexpr (std::is_integral_v<T>) {
                ret = lua_tointeger(L, -1);
            } else if constexpr (std::is_floating_point_v<T>) {
                ret = lua_tonumber(L, -1);
            } else if constexpr (std::is_same_v<T, const char*>) {
                ret = lua_tostring(L, -1);
            } else if constexpr (std::is_same_v<T, std::string>) {
                auto tmp = lua_tostring(L, -1);
                ret = std::string(tmp);
            }
            lua_settop(L, 0);
            return ret;
        }
    }
    template <int RetCount, typename... Args>
    inline void genericpcall(lua_State* L, const char* function, Args const&... args)
    {
        if (function) {
            lua_getglobal(L, function);
            if (!lua_isfunction(L, -1)) {
                lua_settop(L, 0);
                throw std::runtime_error(std::format("Lua function '{}' does not exist", function));
            }
        }
        constexpr const size_t argcount = sizeof...(args);
        if constexpr (argcount > 0) {
            (genericpush(L, args), ...);
        }
        auto status = lua_pcall(L, argcount, RetCount, 0);
        if (status != LUA_OK) {
            auto error = lua_tostring(L, -1);
            lua_settop(L, 0);
            throw std::runtime_error(error);
        }
    }
    template <typename T>
    inline std::optional<T> getglobalv(lua_State* L, const char* name)
    {
        lua_getglobal(L, name);
        return genericget<T>(L);
    }
    template <typename T>
    inline void setglobalv(lua_State* L, const char* name, const T& val)
    {
        genericpush(L, val);
        lua_setglobal(L, name);
    }

    template <typename... Args>
    inline void voidpcall(lua_State* L, const char* function, Args const&... args)
    {
        genericpcall<0>(L, function, args...);
        lua_settop(L, 0);
    }
    template <typename Ret, typename... Args>
    inline std::optional<Ret> pcall1(lua_State* L, const char* function, Args const&... args)
    {
        genericpcall<1>(L, function, args...);
        return genericget<Ret>(L);
    }

    namespace {
        template <typename Optional>
        struct StripOptional {
            using value_type = Optional;
        };

        template <typename T>
        struct StripOptional<std::optional<T>> {
            using value_type = T;
        };

        template <
            /// type of the incoming tuple that will hold the return values of the previous call
            typename Out,
            typename std::size_t N,
            typename Index = std::make_index_sequence<N>>
        inline void set_return_values(Out& ret, lua_State* L)
        {
            [&]<std::size_t... I>(std::index_sequence<I...>) {
                ((std::get<I>(ret) = genericget<typename StripOptional<std::tuple_element_t<I, Out>>::value_type>(L)), ...);
            }(Index {});
        }
        template <typename Tuple>
        struct ApplyOption;

        /// this type transforms a std::tuple<Ts...> into tuple<std::optional<Ts>...>
        template <typename... Ts>
        struct ApplyOption<std::tuple<Ts...>> {
            using value_type = std::tuple<std::optional<Ts>...>;
        };
    }

    template <typename... Elements>
    using return_t = std::tuple<Elements...>;

    template <
        /// Function return signature e.g: std::tuple<int, int>.
        typename RetSig,
        /// Used internally, wraps RetSig into std::optional so that the std::tuple<int, int> from the example
        /// becomes an std::tuple<std::optional<int>, std::optional<int>>. This is the actual return
        /// type of this function.
        typename Ret = ApplyOption<RetSig>::value_type,
        typename... Args>
    inline Ret pcall(lua_State* L, const char* function, Args const&... args)
    {
        constexpr const std::size_t retnum = std::tuple_size_v<RetSig>;
        genericpcall<retnum>(L, function, args...);
        Ret ret {};
        set_return_values<Ret, retnum>(ret, L);
        return ret;
    }
}

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
    void save_solution_for_current_level(std::string&& solution);
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

public:
    explicit EditorWindow();
    virtual void init(Game& game_state) override;
    virtual void update(Game& game_state) override;
    virtual void draw(Game& game_state) override;
    virtual const char* get_window_name() override;
    virtual void set_bounds(Rectangle r) override;
    virtual ~EditorWindow();
    virtual void on_config_reload(const Config& conf) override;

private:
    void update_bounds(void);
};
class LevelSelectWindow final : public Window {
    std::unique_ptr<bed::TextBuffer> m_lvl_text_buffer = nullptr;
    std::unique_ptr<bed::TextBuffer> m_lvl_menu_buffer = nullptr;
    std::unordered_map<std::string, size_t> m_lvl_name_idx_map = {};

    int m_current_level = -1;

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
