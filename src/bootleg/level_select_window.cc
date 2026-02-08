#include "bootleg/game.hpp"
#include <format>
#include <memory>
#include <meu3.h>
#include <print>
#include <raylib.h>
#include <string_view>
#include <unordered_map>

constexpr const std::string display_name_padding = " - ";
constexpr const std::string_view LOAD_LEVEL = "[LOAD LEVEL]";
constexpr const std::string_view CLEAR_SAVED_SOLUTION = "[CLEAR SAVED SOLUTION]";
constexpr const std::string_view LOAD_COMPLETION = "[LOAD COMPLETION]";

namespace boot {
LevelSelectWindow::LevelSelectWindow()
{
}
static Rectangle bounds_for_lvl_tbuf(const Rectangle& w_bounds)
{
    constexpr const float buf_margin = 0.05f;
    const float margin_w = w_bounds.width * buf_margin;
    const float margin_h = w_bounds.height * buf_margin;
    return Rectangle {
        .x = w_bounds.x + margin_w / 2,
        .y = w_bounds.y + margin_h / 2,
        .width = (w_bounds.width / 2) - (margin_w),
        .height = (w_bounds.height) - (margin_h)
    };
}
static Rectangle bounds_for_lvl_menu_tbuf(const Rectangle& w_bounds)
{
    auto tmp = bounds_for_lvl_tbuf(w_bounds);
    tmp.x += w_bounds.width / 2;
    return tmp;
}
void LevelSelectWindow::init(Game& game_state)
{
    m_lvl_text_buffer = std::make_unique<bed::TextBuffer>(game_state.font, m_bounds);
    m_lvl_text_buffer->toggle_readonly();
    m_lvl_text_buffer->toggle_wrap_lines();
    m_lvl_text_buffer->set_bounds(bounds_for_lvl_tbuf(m_bounds));
    m_lvl_text_buffer->insert_line("# Select a level by moving the cursor onto the line with its name and press ENTER");
    m_lvl_text_buffer->insert_line("");

    m_lvl_menu_buffer = std::make_unique<bed::TextBuffer>(game_state.font, m_bounds);
    m_lvl_text_buffer->set_bounds(bounds_for_lvl_menu_tbuf(m_bounds));
    m_lvl_menu_buffer->toggle_readonly();
    m_lvl_menu_buffer->toggle_wrap_lines();

    for (auto i = 1;; i++) {
        const auto lvl_path = std::format("{}/lvl{}.lua", path::LEVELS_DIR, i);
        const auto raw_lvl_path = std::format("{}/lvl{}.raw", path::LEVELS_DIR, i);
        MEU3_Error err = NoError;
        Level lvl = {};
        TraceLog(LOG_DEBUG, "Checking lua level `%s`", lvl_path.data());
        auto has = meu3_package_has(game_state.meu3_pack, lvl_path.data(), &err);
        if (err != NoError) {
            TraceLog(LOG_ERROR, "Error while loading level `%s`, error code: %d", lvl_path.data(), err);
            continue;
        }
        if (has) {
            TraceLog(LOG_DEBUG, "Found level `%s`", lvl_path.data());
            lvl.data_ptr = meu3_package_get_data_ptr(game_state.meu3_pack, lvl_path.data(), &lvl.data_len, &err);
            lvl.ty = Level::Type::Lua;
        } else {
            TraceLog(LOG_DEBUG, "Checking raw level `%s`", raw_lvl_path.data());
            has = meu3_package_has(game_state.meu3_pack, raw_lvl_path.data(), &err);
            if (err != NoError) {
                TraceLog(LOG_ERROR, "Error while loading level `%s`, error code: %d", raw_lvl_path.data(), err);
                continue;
            }
            if (has) {
                TraceLog(LOG_DEBUG, "Found level `%s`", raw_lvl_path.data());
                lvl.data_ptr = meu3_package_get_data_ptr(game_state.meu3_pack, raw_lvl_path.data(), &lvl.data_len, &err);
                lvl.ty = Level::Type::Raw;
            } else {
                break;
            }
        }
        game_state.levels.push_back(std::move(lvl));
        auto display_name = std::format("LEVEL_{:02}", i);
        m_lvl_name_idx_map[display_name] = game_state.levels.size() - 1;
        m_lvl_text_buffer->insert_line(std::format("{}{}", display_name_padding, display_name));
    }
    using namespace std::placeholders;
    m_menu_handlers[LOAD_LEVEL] = std::bind(&LevelSelectWindow::handle_level_load, this, _1);
    m_menu_handlers[CLEAR_SAVED_SOLUTION] = std::bind(&LevelSelectWindow::handle_clear_solution, this, _1);
    m_menu_handlers[LOAD_COMPLETION] = std::bind(&LevelSelectWindow::handle_load_completion, this, _1);
}
void LevelSelectWindow::update(Game& game_state)
{
    if ((IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) && m_lvl_text_buffer->has_focus()) {
        const auto& line = m_lvl_text_buffer->current_line();

        auto pad = line.find(display_name_padding);
        if (pad != std::string::npos) {
            const std::string name(line.begin() + display_name_padding.length(), line.end());
            auto idx = m_lvl_name_idx_map[name];
            m_current_level = idx;
            m_lvl_menu_buffer->clear();
            m_lvl_menu_buffer->insert_line(std::format("# {}", name));
            m_lvl_menu_buffer->insert_newline();
            m_lvl_menu_buffer->insert_line(into<std::string>(LOAD_LEVEL));
            MEU3_Error err = NoError;
            if (meu3_package_has(game_state.meu3_pack,
                    std::format("{}/lvl{}.lua", path::USER_SOLUTIONS_DIR, m_current_level + 1).data(), &err)) {
                m_lvl_menu_buffer->insert_newline();
                m_lvl_menu_buffer->insert_line(into<std::string>(CLEAR_SAVED_SOLUTION));
            }
            if (err != NoError) {
                TraceLog(LOG_ERROR, "Error while checking saved source for level %d", m_current_level + 1);
            }
            err = NoError;
            if (meu3_package_has(game_state.meu3_pack,
                    std::format("{}/lvl{}.lua", path::USER_COMPLETED_DIR, m_current_level + 1).data(), &err)) {
                m_lvl_menu_buffer->insert_line(into<std::string>(LOAD_COMPLETION));
            }
            if (err != NoError) {
                TraceLog(LOG_ERROR, "Error while checking saved solution for level %d", m_current_level + 1);
            }
        }
    } else {
        m_lvl_text_buffer->update_buffer();
    }
    if (m_current_level != -1) {
        m_lvl_menu_buffer->update_buffer();
        if ((IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) && m_lvl_menu_buffer->has_focus()) {
            const auto& line = m_lvl_menu_buffer->current_line();
            if (m_menu_handlers.contains(line)) {
                m_menu_handlers[line](game_state);
            }
        }
    }
}
void LevelSelectWindow::handle_level_load(Game& game_state)
{
    game_state.load_level(game_state.levels[m_current_level], std::format("lvl{}.lua", m_current_level + 1));
    game_state.transition_to("editor");
}
void LevelSelectWindow::handle_clear_solution(Game& game_state)
{
    MEU3_Error err = NoError;
    meu3_package_remove(
        game_state.meu3_pack,
        std::format("{}/lvl{}.lua",
            path::USER_SOLUTIONS_DIR,
            m_current_level + 1)
            .data(),
        &err);
    if (err != NoError) {
        TraceLog(LOG_ERROR, "Error while deleting saved solution for level %d", m_current_level + 1);
    }
    game_state.save_game_data();
}
void LevelSelectWindow::handle_load_completion(Game& game_state)
{
    unsigned long long len = 0;
    MEU3_Error err = NoError;
    if (auto p = meu3_package_get_data_ptr(game_state.meu3_pack,
            std::format("{}/lvl{}.lua", path::USER_SOLUTIONS_DIR, m_current_level + 1).data(),
            &len,
            &err);
        p) {
        game_state.load_level(game_state.levels[m_current_level], std::format("lvl{}.lua", m_current_level + 1));
        game_state.saved_solution = std::string(reinterpret_cast<char*>(p), len);
        game_state.transition_to("editor");
    }
}
void LevelSelectWindow::draw(Game& game_state)
{
    DrawRectangleGradientEx(m_bounds, GREEN, BLUE, GREEN, BLUE);
    m_lvl_text_buffer->draw();
    m_lvl_menu_buffer->draw();
}
const char* LevelSelectWindow::get_window_name()
{
    static std::string name = "levels";
    return name.data();
}
void LevelSelectWindow::set_bounds(Rectangle r)
{
    Window::set_bounds(r);
    if (m_lvl_text_buffer) {
        m_lvl_text_buffer->set_bounds(bounds_for_lvl_tbuf(m_bounds));
    }
    if (m_lvl_menu_buffer) {
        m_lvl_menu_buffer->set_bounds(bounds_for_lvl_menu_tbuf(m_bounds));
    }
}
void LevelSelectWindow::on_config_reload(const Config& conf)
{
    if (m_lvl_text_buffer) {
        m_lvl_text_buffer->foreground_color = conf.foreground_color;
        m_lvl_text_buffer->background_color = conf.background_color;
        m_lvl_text_buffer->set_font_size(conf.font_size);
    }
    if (m_lvl_menu_buffer) {
        m_lvl_menu_buffer->foreground_color = conf.foreground_color;
        m_lvl_menu_buffer->background_color = conf.background_color;
        m_lvl_menu_buffer->set_font_size(conf.font_size);
    }
}
LevelSelectWindow::~LevelSelectWindow()
{
}
}
