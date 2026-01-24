#include <bootleg/game.hpp>
#include <memory>
#include <print>
#include <raylib.h>
#include <raymath.h>
#include <string>

constexpr const float BUFFER_MARGIN = .05;

boot::EditorWindow::EditorWindow()
{
}
void boot::EditorWindow::init(Game& game_state)
{
    m_text_buffer = std::make_unique<bed::TextBuffer>(game_state.m_font, Rectangle {});
    m_text_buffer->insert_string("Color = BLUE");

    Rectangle buffer_bounds = {
        .x = m_bounds.x + m_bounds.width / 2 * BUFFER_MARGIN,
        .y = m_bounds.y + m_bounds.height / 2 * BUFFER_MARGIN,
        .width = (m_bounds.width / 2) - (m_bounds.width * BUFFER_MARGIN),
        .height = (m_bounds.height) - (m_bounds.height * BUFFER_MARGIN),
    };
    m_text_buffer->set_bounds(buffer_bounds);

    m_text_buffer->set_font_size(30);
    m_text_buffer->toggle_wrap_lines();
    // m_text_buffer->set_height(m_bounds.height);
    // m_text_buffer->set_width(m_bounds.width / 2);
    // m_text_buffer->set_position({ m_bounds.x, m_bounds.y });
    m_camera.position = (Vector3) { 20.0f, 10.0f, 0.0f };
    m_camera.target = { 0, 0, 0 };
    m_camera.up = (Vector3) { 0.0f, 1.0f, 0.0 };
    m_camera.fovy = 90.0f;
    m_camera.projection = CAMERA_PERSPECTIVE;
    m_render_tex = LoadRenderTexture(m_render_tex_dims.x, m_render_tex_dims.y);
    m_cube_bounds = {
        .x = m_bounds.x + this->m_bounds.width / 2,
        .y = m_bounds.y,
        .width = this->m_bounds.width / 2,
        .height = this->m_bounds.height / 2,
    };
}
boot::EditorWindow::~EditorWindow()
{
    UnloadRenderTexture(m_render_tex);
}
void boot::EditorWindow::update(Game& game_state)
{
    static bool cube_clicked = false;
    const auto mouse = GetMousePosition();
    cube_clicked = (CheckCollisionPointRec(mouse, m_cube_bounds) && IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
        || (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && cube_clicked);
    if (cube_clicked) {
        UpdateCamera(&m_camera, CAMERA_THIRD_PERSON);
    } else if (IsKeyPressed(KEY_ENTER) && AnySpecialDown(SHIFT)) {
        game_state.load_source(m_text_buffer->get_contents_as_string());
    } else {
        this->m_text_buffer->update_buffer();
    }
}
void boot::EditorWindow::draw(Game& game_state)
{
    Rectangle buffer_bounds = {
        .x = m_bounds.x,
        .y = m_bounds.y,
        .width = (m_bounds.width / 2),
        .height = (m_bounds.height),
    };
    DrawRectangleGradientEx(buffer_bounds, RED, BLUE, RED, BLUE);
    this->m_text_buffer->draw();
    BeginTextureMode(m_render_tex);
    ClearBackground(WHITE);
    BeginMode3D(m_camera);
    const auto& cube = game_state.cube;
    const auto brick_width = 1.0;
    for (int x = -(cube.x / 2); x < (cube.x / 2); x++) {
        for (int y = brick_width / 2; y < brick_width + cube.y; y++) {
            for (int z = -(cube.z / 2); z < (cube.z / 2); z++) {
                Color c = game_state.color_for(
                    x + (cube.x / 2),
                    y - brick_width / 2,
                    z + cube.z / 2);
                Vector3 pos = (Vector3) { (float)x, (float)y, (float)z };
                if (c.a == 255)
                    DrawCube(pos, brick_width, brick_width, brick_width, c);
            }
        }
    }
    DrawGrid(5, 5);
    EndMode3D();
    EndTextureMode();
    Rectangle src = {
        .x = 0,
        .y = 0,
        .width = m_render_tex_dims.x,
        .height = m_render_tex_dims.y,
    };
    DrawTexturePro(m_render_tex.texture,
        src,
        m_cube_bounds,
        { this->m_bounds.width / 2, this->m_bounds.height / 2 },
        180.0,
        WHITE);
}
const char* boot::EditorWindow::get_window_name()
{
    static std::string name = "editor";
    return name.data();
}
void boot::EditorWindow::set_bounds(Rectangle r)
{
    boot::Window::set_bounds(r);
    if (m_text_buffer) {
        Rectangle buffer_bounds = {
            .x = m_bounds.x + m_bounds.width / 2 * BUFFER_MARGIN,
            .y = m_bounds.y + m_bounds.height / 2 * BUFFER_MARGIN,
            .width = (m_bounds.width / 2) - (m_bounds.width * BUFFER_MARGIN),
            .height = (m_bounds.height) - (m_bounds.height * BUFFER_MARGIN),
        };
        m_text_buffer->set_bounds(buffer_bounds);
    }
    m_cube_bounds = {
        .x = m_bounds.x + this->m_bounds.width / 2,
        .y = m_bounds.y,
        .width = this->m_bounds.width / 2,
        .height = this->m_bounds.height / 2,
    };
}
