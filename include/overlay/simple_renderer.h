#pragma once

#include <string>

struct OverlayFrame {
    std::string title;
    bool menu_open{false};
    int surface_width{1080};
    int surface_height{2400};
};

class SimpleRenderer {
public:
    bool initialize();
    void render_frame(const OverlayFrame &frame);
    void shutdown();
};

