#include <bootleg/game.hpp>
#include <iostream>
#include <memory>
#include <optional>
#include <print>
#include <raylib.h>
#include <raymath.h>
#include <string>
#include <string_view>

constexpr const float BUFFER_MARGIN = .05;

using buffer_t = bed::TextBuffer;
static void process_syntax(Color foreground, buffer_t::syntax_data_t& syntax, buffer_t::text_buffer_iterator tit, const buffer_t::text_buffer_iterator end);

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
    m_slider.set_bounds({
        .x = m_cube_bounds.x,
        .y = m_cube_bounds.y,
        .width = m_cube_bounds.width * 0.05f,
        .height = m_cube_bounds.height,
    });
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
    m_slider = { {}, 0, 1000, 1000 };
    m_slider.bar_color = Color { 0x1f, 0x1f, 0x1f, 80 };
    m_slider.slider_color = YELLOW;
    m_slider.set_value(m_slider.get_max());
    update_bounds();
}
boot::EditorWindow::~EditorWindow()
{
    UnloadRenderTexture(m_render_tex);
}
void boot::EditorWindow::update(Game& game_state)
{
    static bool cube_clicked = false;
    if (game_state.saved_solution) {
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
        if (game_state.level_completed) {
            m_output_buffer->insert_string("Level solved!");
            auto sol = m_text_buffer->get_contents_as_string();
            const size_t len = sol.length();
            auto ret = game_state.save_solution_for_current_level(std::move(sol));
            if (ret) {
                auto msg = std::format("[NEW SMALLEST ({} bytes) SOLUTION SAVED]", len);
                m_output_buffer->insert_string(std::move(msg));
            }
        }
    } else {
        this->m_text_buffer->update_buffer();
    }
    if (IsKeyPressed(KEY_S) && AnySpecialDown(CONTROL)) {
        game_state.save_source_for_current_level(m_text_buffer->get_contents_as_string());
    }
    this->m_slider.update();
    this->m_output_buffer->update_buffer();
}

void boot::EditorWindow::draw(Game& game_state)
{
    DrawRectangleGradientEx(m_bounds, RED, BLUE, RED, BLUE);
    this->m_text_buffer->draw();
    this->m_output_buffer->draw();
    BeginTextureMode(m_render_tex);
    BeginBlendMode(BLEND_ALPHA);
    ClearBackground(WHITE);
    BeginMode3D(m_camera);
    const auto& cube = game_state.cube;
    const auto brick_width = 1.0;
    const int layer = cube.y * m_slider.get_percentage() + 1;
    for (int x = 0; x < cube.x; x++) {
        for (int y = 0; y < cube.y && y < layer; y++) {
            for (int z = 0; z < cube.z; z++) {
                auto nx = x - ((cube.x - 1) * brick_width / 2);
                auto nz = z - ((cube.z - 1) * brick_width / 2);
                auto ny = y + brick_width / 2;
                Color c = game_state.color_for(
                    x,
                    y,
                    z);
                Vector3 pos = (Vector3) { (float)nx, (float)ny, (float)nz };

                if (c.a == 255) {
                    if (game_state.m_solution) {
                        const auto s = game_state.m_solution->color_data[x][y][z];
                        if ((c.r != s.r || c.g != s.g || c.b != s.b) && s.a != 0 && c.a != 0) {
                            DrawCube(pos, brick_width / 3, brick_width / 3, brick_width / 3, RED);
                        } else {
                            DrawCube(pos, brick_width, brick_width, brick_width, c);
                        }
                    } else {
                        DrawCube(pos, brick_width, brick_width, brick_width, c);
                    }
                } else if (game_state.m_solution) {
                    const auto scolor = game_state.m_solution->color_data[x][y][z];
                    if (scolor.a)
                        DrawCube(pos, brick_width / 3, brick_width / 3, brick_width / 3, { scolor.r, scolor.g, scolor.b, 255 });
                }

                char str[] = "";
                int csz = 1;
                if (y == 0 && z == 0) {
                    str[0] = '0' + x;
                    int c = GetCodepoint(str, &csz);
                    auto sz = boot::measure_codepoint_3d(c, game_state.font, 0.8);
                    auto mark_pos = pos;
                    mark_pos.x -= sz.x / 2;
                    mark_pos.y = 0;
                    mark_pos.z -= sz.y * 2;
                    boot::draw_codepoint_3d(c, game_state.font, mark_pos,
                        0.8, boot::colors::X_AXIS,
                        false, Rotation::x_axis(-90));
                }
                if (y == 0 && x == 0) {
                    str[0] = '0' + z;
                    int c = GetCodepoint(str, &csz);
                    auto sz = boot::measure_codepoint_3d(c, game_state.font, 0.8);
                    auto mark_pos = pos;
                    mark_pos.x -= sz.y * 2;
                    mark_pos.y = 0;
                    mark_pos.z -= sz.x / 2;
                    boot::draw_codepoint_3d(c, game_state.font, mark_pos,
                        0.8, boot::colors::Z_AXIS,
                        false, Rotation::x_axis(-90));
                }
                if (z == 0 && x == 0) {
                    str[0] = '0' + y;
                    int c = GetCodepoint(str, &csz);
                    auto sz = boot::measure_codepoint_3d(c, game_state.font, 0.8);
                    auto mark_pos = pos;
                    // mark_pos.x -= sz.y * 2;
                    mark_pos.y += sz.y / 2;
                    mark_pos.x -= brick_width + sz.x / 2;
                    mark_pos.z -= brick_width;
                    boot::draw_codepoint_3d(c, game_state.font, mark_pos,
                        0.8, boot::colors::Y_AXIS,
                        false, Rotation::y_axis(45));
                }
            }
        }
    }
    DrawGrid(5, 5);
    constexpr const auto axis_len = 20;
    const Vector3 axis_center = { -2 * 5., 0, -2 * 5. };
    DrawLine3D(axis_center, Vector3Add(axis_center, { axis_len, 0, 0 }), boot::colors::X_AXIS);
    DrawLine3D(axis_center, Vector3Add(axis_center, { 0, axis_len, 0 }), boot::colors::Y_AXIS);
    DrawLine3D(axis_center, Vector3Add(axis_center, { 0, 0, axis_len }), boot::colors::Z_AXIS);

    constexpr const auto axis_mark_size = 5;
    int csz = 1;
    int c = GetCodepoint("X", &csz);
    auto sz = boot::measure_codepoint_3d(c, game_state.font, axis_mark_size);
    boot::draw_codepoint_3d(c, game_state.font, { axis_center.x + axis_len - sz.x, axis_center.y + sz.y, axis_center.z },
        axis_mark_size, boot::colors::X_AXIS,
        false);
    c = GetCodepoint("Z", &csz);
    sz = boot::measure_codepoint_3d(c, game_state.font, axis_mark_size);
    boot::draw_codepoint_3d(c, game_state.font, { axis_center.x, axis_center.y + sz.y, axis_center.z + axis_len },
        axis_mark_size, boot::colors::Z_AXIS,
        false, Rotation::y_axis(90));
    c = GetCodepoint("Y", &csz);
    sz = boot::measure_codepoint_3d(c, game_state.font, axis_mark_size);
    boot::draw_codepoint_3d(c, game_state.font, { axis_center.x + 0.5f, axis_center.y + axis_len, axis_center.z },
        axis_mark_size, boot::colors::Y_AXIS,
        false, Rotation::none());

    EndMode3D();
    EndBlendMode();
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
    this->m_slider.draw();
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
        if (conf.syntax_highlighting){
            auto ps = std::bind(process_syntax, conf.foreground_color, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
            m_text_buffer->set_syntax_parser(ps);
        }
        else
            m_text_buffer->set_syntax_parser(nullptr);
    }
}
void boot::EditorWindow::on_transition(Game& game_state)
{
    if (m_output_buffer) {
        m_output_buffer->clear();
    }
    m_slider.set_value(m_slider.get_max());
}

namespace tokens {
static const Color DIGIT = boot::decode_color_from_hex(0xB4CCA1FF);
static const Color ROUND_PAREN = boot::decode_color_from_hex(0xDBD996FF);
static const Color KEYWORD_PURPLE = boot::decode_color_from_hex(0xC185BCFF);
static const Color KEYWORD_BLUE = boot::decode_color_from_hex(0x4194D4FF);
static const Color COLOR = boot::decode_color_from_hex(0x4EC37FFF);
};

static std::optional<Color> match_literal(const std::string_view lit)
{
    if (lit == "Color")
        return tokens::COLOR;
    if (lit == "then" || lit == "else" || lit == "if" || lit == "elseif" || lit == "end" || lit == "return" || lit == "local")
        return tokens::KEYWORD_PURPLE;
    else if (lit == "function" || lit == "or" || lit == "and")
        return tokens::KEYWORD_BLUE;
    else if (lit == "X" || lit == "x")
        return boot::colors::X_AXIS;
    else if (lit == "Y" || lit == "y")
        return boot::colors::Y_AXIS;
    else if (lit == "Z" || lit == "z")
        return boot::colors::Z_AXIS;
    else if (lit == "BLANK")
        return WHITE;
    else if (boot::colors::COLORMAP.contains(lit))
        return boot::colors::COLORMAP.at(lit);
    return std::nullopt;
}
static bool hex_dig_check(char c)
{
    switch (c) {
        // clang-format off
    case 'a': case 'A':
    case 'b': case 'B':
    case 'c': case 'C':
    case 'd': case 'D':
    case 'e': case 'E':
    case 'f': case 'F':
        // clang-format on
        return true;
    default:
        return false;
    };
};

static void process_syntax(Color foreground,
    buffer_t::syntax_data_t& syntax,
    buffer_t::text_buffer_iterator tit,
    const buffer_t::text_buffer_iterator end)
{
    std::string buf;
    buf.reserve(20);
    buffer_t::Cursor pos = tit.current_cursor_pos();
    for (; tit != end;) {
        bool dig_has_dot = false;
        bool dig_has_x = false;
        // bool dig_has_minus = false;
        pos = tit.current_cursor_pos();
        buffer_t::char_t c = *tit;
        switch (c) {
        case '(':
        case ')':
            syntax[pos] = tokens::ROUND_PAREN;
            break;
        case '.':
            dig_has_dot = true;
            tit++;
        // case '-':
        // case '+':
        //     tit++;
        //     if (tit == end && !std::isdigit(*tit))
        //         break;
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case '0':
            syntax[pos] = tokens::DIGIT;
            for (; tit != end; tit++) {
                const auto ch = *tit;
                if (ch == '.' && dig_has_dot) {
                    syntax[pos] = foreground;
                    break;
                }
                dig_has_dot = ch == '.';
                // if ((ch == 'X' || ch == 'x') && !dig_has_x && !dig_has_dot) {
                //     dig_has_x = true;
                //     continue;
                // }
                // const auto is_valid_hex = (dig_has_x && hex_dig_check(ch)) || !dig_has_x;
                // if (!std::isdigit(ch) && !is_valid_hex) {
                //     goto skip_pos_increment;
                // }
                if (!std::isdigit(ch)) {
                    goto skip_pos_increment;
                }
            }
            break;
        case ' ':
        case '\t':
        case '\n':
            syntax[pos] = foreground;
            break;
            // clang-format off
        case 97: case 98: case 99: case 100:
        case 101: case 102: case 103: case 104:
        case 105: case 106: case 107: case 108:
        case 109: case 110: case 111: case 112:
        case 113: case 114: case 115: case 116:
        case 117: case 118: case 119: case 120:
        case 121: case 122: case 65: case 66:
        case 67: case 68: case 69: case 70:
        case 71: case 72: case 73: case 74:
        case 75: case 76: case 77: case 78:
        case 79: case 80: case 81: case 82:
        case 83: case 84: case 85: case 86:
        case 87: case 88: case 89: case 90:
        // clang-format on
        case '_':
            syntax[pos] = foreground;
            buf.clear();
            for (; tit != end; tit++) {
                const auto ch = *tit;
                if ((std::isspace(ch) || !std::isalnum(ch) || ch == '\n') && ch != '_') {
                    syntax[pos] = match_literal(buf).value_or(foreground);
                    goto skip_pos_increment;
                }
                buf.push_back(ch);
            }
            break;
        default:
            syntax[pos] = foreground;
            break;
        }
        tit++;
    skip_pos_increment:
    }
}
