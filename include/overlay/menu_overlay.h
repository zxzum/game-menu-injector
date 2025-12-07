#pragma once

#include "overlay/simple_renderer.h"
#include "overlay/input_listener.h"

#include <atomic>
#include <csignal>
#include <functional>
#include <thread>
#include <mutex>

class MenuOverlay {
public:
    explicit MenuOverlay(std::string title);
    ~MenuOverlay();

    bool initialize();
    void request_toggle();
    void run();
    void stop();

private:
    static void handle_signal(int sig);

    void loop();
    void on_touch(const TouchEvent &ev);
    OverlayFrame snapshot_frame();

    std::string title_;
    SimpleRenderer renderer_{};
    InputListener input_{};
    std::atomic<bool> running_{false};
    std::atomic<bool> menu_open_{false};
    std::thread worker_{};

    int screen_w_{1080};
    int screen_h_{2400};
    int menu_x_{0};
    int menu_y_{0};
    int menu_w_{240};
    int menu_h_{100};
    bool dragging_{false};
    int drag_offset_x_{0};
    int drag_offset_y_{0};

    std::mutex state_mutex_{};
};

