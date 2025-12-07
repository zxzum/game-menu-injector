#pragma once

#include <string>

struct OverlayFrame {
    std::string title;
    bool menu_open{false};
    int surface_width{1080};
    int surface_height{2400};
    int menu_x{0};
    int menu_y{0};
    int menu_w{220};
    int menu_h{80};
    bool dragging{false};
};

class SimpleRenderer {
public:
    bool initialize();
    void render_frame(const OverlayFrame &frame);
    void shutdown();

    int width() const { return width_; }
    int height() const { return height_; }

private:
    void draw_rect(uint8_t *buffer, int x, int y, int w, int h, uint32_t color, uint8_t alpha);
    void draw_text(uint8_t *buffer, int x, int y, const std::string &text, uint32_t color, uint8_t alpha);
    void blend_pixel(uint8_t *pixel_ptr, uint32_t color, uint8_t alpha);
    uint32_t read_pixel(const uint8_t *pixel_ptr) const;
    void write_pixel(uint8_t *pixel_ptr, uint32_t color);

    int fd_{-1};
    int width_{1080};
    int height_{2400};
    int bpp_{32};
    size_t line_length_{0};
    size_t buffer_size_{0};
    uint8_t *fb_map_{nullptr};
    bool mapped_{false};
};

