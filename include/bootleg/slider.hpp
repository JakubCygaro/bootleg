#ifndef SLIDER_HPP
#define SLIDER_HPP
#include <raylib.h>
namespace boot {
class Slider {
    enum class Type {
        VERTICAL,
        HORIZONTAL
    };
    Type m_type = Slider::Type::VERTICAL;
    Rectangle m_bounds {};
    double m_max_val{};
    double m_min_val{};
    double m_val{};
    double m_prop{};

public:
    Color bar_color{};
    Color slider_color{};

public:
    Slider();
    Slider(Rectangle bounds, double min = 0, double max = 100, double val = 0);
    void set_vertical(void);
    void set_horizontal(void);
    bool is_vertiacal(void) const;
    void set_value(double val);
    double get_value(void) const;
    void set_max(double max);
    double get_max(void) const;
    void set_min(double min);
    double get_min(void) const;
    void set_bounds(Rectangle bounds);
    Rectangle get_bounds(void) const;
    void update();
    void draw();
};
}
#endif
