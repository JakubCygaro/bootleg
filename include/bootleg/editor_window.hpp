#ifndef BOOT_EDITOR_HPP
#define BOOT_EDITOR_HPP

#include "buffer.hpp"
#include <bootleg/game.hpp>
#include <raylib.h>

namespace boot {
class EditorWindow final : public Window {
    bed::TextBuffer m_text_buffer;

public:
    explicit EditorWindow(Font font, Rectangle bounds);
    virtual void update(Game& game_state) override;
    virtual void draw(Game& game_state) override;
    virtual const char* get_window_name() override;
};
}

#endif
