#include <bootleg/game.hpp>
#include <memory>
#include <optional>
#include <print>
#include <raylib.h>
#include <raymath.h>
#include <string>

constexpr const float BUFFER_MARGIN = .05;

boot::EditorWindow::EditorWindow()
{
}
void boot::EditorWindow::update_bounds(void)
{
    m_cube_bounds = {
        .x = m_bounds.x + this->m_bounds.width / 2,
        .y = m_bounds.y + m_bounds.height / 2 * BUFFER_MARGIN,
        .width = (this->m_bounds.width / 2) - (m_bounds.width / 2 * BUFFER_MARGIN),
        .height = this->m_bounds.height / 2,
    };
    if (m_text_buffer) {
        Rectangle buffer_bounds = {
            .x = m_bounds.x + m_bounds.width / 2 * BUFFER_MARGIN,
            .y = m_bounds.y + m_bounds.height / 2 * BUFFER_MARGIN,
            .width = (m_bounds.width / 2) - (m_bounds.width * BUFFER_MARGIN),
            .height = (m_bounds.height) - (m_bounds.height * BUFFER_MARGIN),
        };
        m_text_buffer->set_bounds(buffer_bounds);
    }
    if (m_output_buffer) {
        Rectangle buffer_bounds = m_cube_bounds;
        buffer_bounds.y += m_bounds.height / 2 + (m_bounds.height / 2 * BUFFER_MARGIN);
        buffer_bounds.height = (m_bounds.height - buffer_bounds.y); // - (m_bounds.height / 2 * BUFFER_MARGIN );
        m_output_buffer->set_bounds(buffer_bounds);
    }
}
void boot::EditorWindow::init(Game& game_state)
{
    m_text_buffer = std::make_unique<bed::TextBuffer>(game_state.font, Rectangle {});
    m_output_buffer = std::make_unique<bed::TextBuffer>(game_state.font, Rectangle {});

    m_text_buffer->insert_string("Color = BLUE");
    m_text_buffer->set_font_size(30);

    m_output_buffer->set_font_size(30);
    m_output_buffer->toggle_wrap_lines();
    m_output_buffer->toggle_readonly();
    m_output_buffer->toggle_cursor();
    m_camera.position = (Vector3) { 20.0f, 10.0f, 0.0f };
    m_camera.target = { 0, 0, 0 };
    m_camera.up = (Vector3) { 0.0f, 1.0f, 0.0 };
    m_camera.fovy = 90.0f;
    m_camera.projection = CAMERA_PERSPECTIVE;
    m_render_tex = LoadRenderTexture(m_render_tex_dims.x, m_render_tex_dims.y);
    SetTextureFilter(m_render_tex.texture, TEXTURE_FILTER_ANISOTROPIC_4X);
    update_bounds();
}
boot::EditorWindow::~EditorWindow()
{
    UnloadRenderTexture(m_render_tex);
}
void boot::EditorWindow::update(Game& game_state)
{
    static bool cube_clicked = false;
    if(game_state.saved_solution){
        m_text_buffer->clear();
        m_text_buffer->insert_string(std::move(*game_state.saved_solution));
        game_state.saved_solution = std::nullopt;
    }
    const auto mouse = GetMousePosition();
    cube_clicked = (CheckCollisionPointRec(mouse, m_cube_bounds) && IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
        || (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && cube_clicked);
    if (cube_clicked) {
        UpdateCamera(&m_camera, CAMERA_THIRD_PERSON);
    }
    if (IsKeyPressed(KEY_ENTER) && AnySpecialDown(SHIFT)) {
        m_output_buffer->clear();
        if (auto err = game_state.load_source(m_text_buffer->get_contents_as_string()); err) {
            auto errstr = *err;
            m_output_buffer->insert_string(std::move(errstr));
        }
    } else {
        this->m_text_buffer->update_buffer();
    }
    if (IsKeyPressed(KEY_S) && AnySpecialDown(CONTROL)) {
        game_state.save_solution_for_current_level(m_text_buffer->get_contents_as_string());
    }
    this->m_output_buffer->update_buffer();
}
void boot::EditorWindow::draw(Game& game_state)
{
    DrawRectangleGradientEx(m_bounds, RED, BLUE, RED, BLUE);
    this->m_text_buffer->draw();
    this->m_output_buffer->draw();
    BeginTextureMode(m_render_tex);
    ClearBackground(WHITE);
    BeginMode3D(m_camera);
    const auto& cube = game_state.cube;
    const auto brick_width = 1.0;
    for (int x = 0; x < cube.x; x++) {
        for (int y = 0; y < cube.y; y++) {
            for (int z = 0; z < cube.z; z++) {
                auto nx = x - ((cube.x - 1) * brick_width / 2);
                auto nz = z - ((cube.z - 1) * brick_width / 2);
                auto ny = y + brick_width / 2;
                Color c = game_state.color_for(
                    x,
                    y,
                    z);
                Vector3 pos = (Vector3) { (float)nx, (float)ny, (float)nz };
                if (c.a == 255)
                    DrawCube(pos, brick_width, brick_width, brick_width, c);
                else if (game_state.solution) {
                    const auto scolor = game_state.solution->color_data[x][y][z];
                    if(scolor.a)
                        DrawCube(pos, brick_width, brick_width, brick_width, { scolor.r, scolor.g, scolor.b, 80 });
                }
            }
        }
    }
    DrawGrid(5, 5);
    const Vector3 axis_center = { -2 * 5., 0, -2 * 5. };
    DrawLine3D(axis_center, Vector3Add(axis_center, { 20, 0, 0 }), RED);
    DrawLine3D(axis_center, Vector3Add(axis_center, { 0, 20, 0 }), GREEN);
    DrawLine3D(axis_center, Vector3Add(axis_center, { 0, 0, 20 }), BLUE);
    EndMode3D();
    EndTextureMode();
    Rectangle src = {
        .x = 0,
        .y = 0,
        .width = m_render_tex_dims.x,
        .height = -m_render_tex_dims.y,
    };
    DrawTexturePro(m_render_tex.texture,
        src,
        m_cube_bounds,
        Vector2Zero(),
        0,
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
    update_bounds();
}
void boot::EditorWindow::on_config_reload(const Config& conf)
{
    if (m_text_buffer) {
        m_text_buffer->foreground_color = conf.foreground_color;
        m_text_buffer->background_color = conf.background_color;
        m_text_buffer->set_font_size(conf.font_size);
        m_text_buffer->wrap_lines(conf.wrap_lines);
    }
}
