#include "bootleg/slider.hpp"
#include <algorithm>
#include <iostream>
#include <raylib.h>
#include <raymath.h>
namespace boot {
Slider::Slider()
{
}
Slider::Slider(Rectangle bounds, double min, double max, double val)
    : m_bounds(bounds)
    , m_max_val(max)
    , m_min_val(min)
    , m_val(val)
{
}
void Slider::set_vertical(void)
{
    m_type = Type::VERTICAL;
}
void Slider::set_horizontal(void)
{
    m_type = Type::HORIZONTAL;
}
bool Slider::is_vertiacal(void) const
{
    return m_type == Type::VERTICAL;
}
void Slider::set_value(double val)
{
    m_val = std::clamp(val, m_min_val, m_max_val);
}
double Slider::get_value(void) const
{
    return m_val;
}
void Slider::set_max(double max)
{
    m_max_val = max >= m_min_val ? max : m_max_val;
}
double Slider::get_max(void) const
{
    return m_max_val;
}
void Slider::set_min(double min)
{
    m_min_val = min <= m_max_val ? min : m_min_val;
}
double Slider::get_min(void) const
{
    return m_min_val;
}
double Slider::get_percentage(void) const{
    return m_prop;
}
void Slider::set_bounds(Rectangle bounds)
{
    m_bounds = bounds;
}
Rectangle Slider::get_bounds(void) const
{
    return m_bounds;
}
constexpr const float slider_sz = 0.1f;
void Slider::update()
{
    const auto mouse = GetMousePosition();
    static bool active = false;
    active = (CheckCollisionPointRec(mouse, m_bounds) && IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        || (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && active);
    if (!active)
        return;
    const float slider_v = slider_sz * m_bounds.height;
    const float slider_h = slider_sz * m_bounds.width;
    const auto point = Vector2Subtract(mouse, Vector2 { m_bounds.x, m_bounds.y + slider_v / 2 });
    if (m_type == Type::VERTICAL) {
        const auto y = (m_bounds.height - slider_v) - std::clamp(point.y, 0.0f, m_bounds.height - slider_v);
        const auto prop = y / (m_bounds.height - slider_v);
        m_prop = prop;
    } else {
        const auto x = (m_bounds.width - slider_h) - std::clamp(point.x, 0.0f, m_bounds.width - slider_h);
        const auto prop = x / (m_bounds.width - slider_h);
        m_prop = prop;
    }
    m_val = m_min_val + (m_prop * std::abs(m_min_val - m_max_val));
}
void Slider::draw()
{
    DrawRectangleRounded(m_bounds, 0.2, 10, bar_color);
    Rectangle slider {};
    if (m_type == Type::VERTICAL) {
        const float slider_v = slider_sz * m_bounds.height;
        slider = Rectangle {
            .x = m_bounds.x,
            .y = (m_bounds.y + m_bounds.height - slider_v) - (float)((m_bounds.height - slider_v) * m_prop),
            .width = m_bounds.width,
            .height = slider_v,
        };
    } else {
        const float slider_h = slider_sz * m_bounds.width;
        slider = Rectangle {
            .x = (m_bounds.x + m_bounds.width - slider_h) - (float)((m_bounds.width - slider_h) * m_prop),
            .y = m_bounds.y,
            .width = slider_h,
            .height = m_bounds.height,
        };
    }
    DrawRectangleRounded(slider, 0.2, 10, slider_color);
}
}
