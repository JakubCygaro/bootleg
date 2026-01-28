#ifndef BOOT_GAME_HPP
#define BOOT_GAME_HPP
#include <buffer.hpp>
#ifdef __cplusplus
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}
#endif
#include <memory>
#include <raylib.h>
#include <vector>

#ifndef STRINGIFY
#define STRINGIFY(TOKEN) #TOKEN
#endif
namespace boot {
namespace colors {
    constexpr const unsigned int CBLANK = 0x00000000;
    constexpr const unsigned int CRED = 0xff0000ff;
    constexpr const unsigned int CGREEN = 0x00ff00ff;
    constexpr const unsigned int CBLUE = 0x0000ffff;
    constexpr const unsigned int CMAGENTA = 0xff00ffff;
    constexpr const unsigned int CORANGE = 0xffb300ff;
    constexpr const unsigned int CYELLOW = 0xf7ff00ff;
    constexpr const unsigned int CBLACK = 0xffffffff;
}
class Game;

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

class Game {
    Vector2 m_dims {};
    lua_State* m_lua_state {};

public:
    Font m_font;
    CubeData cube {};
    inline Game(float w, float h)
        : m_dims(w, h)
    {
    }
    // windows? I'm gonna puke
    std::vector<std::unique_ptr<Window>> windows {};
    void init();
    void deinit();
    void update();
    void draw();
    std::optional<std::string> load_source(const std::string&);
    Color color_for(int x, int y, int z);

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
private:
    void update_bounds(void);
};
}
#endif
