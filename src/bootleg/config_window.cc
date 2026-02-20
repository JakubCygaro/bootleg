#include "bootleg/game.hpp"
#include <memory>
#include <meu3.h>
namespace boot {
ConfigWindow::ConfigWindow() { }
static Rectangle bounds_for_conf_tbuf(const Rectangle& w_bounds)
{
    constexpr const float buf_margin = 0.05f;
    const float margin_w = w_bounds.width * buf_margin;
    const float margin_h = w_bounds.height * buf_margin;
    return Rectangle { .x = w_bounds.x + margin_w / 2,
        .y = w_bounds.y + margin_h / 2,
        .width = (w_bounds.width) - (margin_w),
        .height = (w_bounds.height) - (margin_h) };
}
void ConfigWindow::init(Game& game_state)
{
    m_config_text_buffer = std::make_unique<bed::TextBuffer>(
        game_state.font, bounds_for_conf_tbuf(m_bounds));

    // try to load user config if it exists
    MEU3_Error err = NoError;
    auto has = meu3_package_has(game_state.meu3_pack, path::USER_CONFIG.data(), &err);
    if (err != NoError) {
        TraceLog(LOG_ERROR, "Error while checking for user config presence");
    } else if (has) {
        unsigned long long len = 0;
        auto conf = (const char*)meu3_package_get_data_ptr(
            game_state.meu3_pack, path::USER_CONFIG.data(), &len, &err);
        if (err != NoError) {
            TraceLog(LOG_ERROR, "Error while trying to get a ref for user config");
        } else {
            m_config_text_buffer->insert_string(std::string(conf, len));
            m_config_text_buffer->jump_cursor_to_top();
        }
    } else {
        unsigned long long len = 0;
        auto def_conf = (const char*)meu3_package_get_data_ptr(
            game_state.meu3_pack, path::DEF_CONFIG.data(), &len, &err);
        if (err != NoError) {
            TraceLog(LOG_ERROR, "Error while trying to get a ref for default config");
        } else {
            m_config_text_buffer->insert_string(std::string(def_conf, len));
            m_config_text_buffer->jump_cursor_to_top();
        }
    }
};
void ConfigWindow::update(Game& game_state)
{
    if (IsKeyPressed(KEY_ENTER) && AnySpecialDown(SHIFT)) {
        game_state.reload_configuration(
            m_config_text_buffer->get_contents_as_string());
    } else if (IsKeyPressed(KEY_S) && AnySpecialDown(CONTROL)) {
        auto conf = m_config_text_buffer->get_contents_as_string();
        MEU3_Error err = NoError;
        meu3_package_insert(game_state.meu3_pack, path::USER_CONFIG.data(),
            (MEU3_BYTES)conf.data(), conf.size(), &err);
        if (err != NoError) {
            TraceLog(LOG_ERROR, "Failed to save user configuration");
        }
        game_state.save_game_data();
    } else if (IsKeyPressed(KEY_R) && AnySpecialDown(CONTROL)) {
        MEU3_Error err = NoError;
        unsigned long long len = 0;
        auto def_conf = (const char*)meu3_package_get_data_ptr(
            game_state.meu3_pack, path::DEF_CONFIG.data(), &len, &err);
        if (err != NoError) {
            TraceLog(LOG_ERROR, "Error while trying to get a ref for default config");
        } else {
            m_config_text_buffer->clear();
            m_config_text_buffer->insert_string(std::string(def_conf, len));
            m_config_text_buffer->jump_cursor_to_top();
        }
    } else {
        m_config_text_buffer->update_buffer();
    }
}
void ConfigWindow::draw(Game& game_state)
{
    DrawRectangleGradientEx(m_bounds, PURPLE, ORANGE, PURPLE, ORANGE);
    m_config_text_buffer->draw();
}
const char* ConfigWindow::get_window_name()
{
    static std::string name = "config";
    return name.data();
}
void ConfigWindow::set_bounds(Rectangle r)
{
    Window::set_bounds(r);
    if (m_config_text_buffer)
        m_config_text_buffer->set_bounds(bounds_for_conf_tbuf(m_bounds));
}
ConfigWindow::~ConfigWindow() { }
void ConfigWindow::on_config_reload(const Config& conf)
{
    if (m_config_text_buffer) {
        m_config_text_buffer->foreground_color = conf.foreground_color;
        m_config_text_buffer->background_color = conf.background_color;
        m_config_text_buffer->set_font_size(conf.font_size);
        m_config_text_buffer->wrap_lines(conf.wrap_lines);
    }
}
} // namespace boot
