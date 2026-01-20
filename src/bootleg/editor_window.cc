#include <bootleg/editor_window.hpp>
#include <string>
boot::EditorWindow::EditorWindow(Font font, Rectangle bounds)
    : m_text_buffer(font, bounds)
{
    m_text_buffer.insert_string("sperma");
    m_text_buffer.set_font_size(50);
}
void boot::EditorWindow::update(Game& game_state)
{
    this->m_text_buffer.update_buffer();
}
void boot::EditorWindow::draw(Game& game_state)
{
    this->m_text_buffer.draw();
}
const char* boot::EditorWindow::get_window_name()
{
    static std::string name = "editor";
    return name.data();
}
