#ifndef BOOT_GAME_HPP
#define BOOT_GAME_HPP
#include <buffer.hpp>
#include <memory>
#include <raylib.h>
#include <vector>
namespace boot {
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
};

class Game {
    Vector2 m_dims {};

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
};
class EditorWindow final : public Window {
    std::unique_ptr<bed::TextBuffer> m_text_buffer;
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
};
}
#endif
