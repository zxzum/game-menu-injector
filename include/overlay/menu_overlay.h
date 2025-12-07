#pragma once

#include "overlay/simple_renderer.h"

#include <atomic>
#include <csignal>
#include <functional>
#include <thread>

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

    std::string title_;
    SimpleRenderer renderer_{};
    std::atomic<bool> running_{false};
    std::atomic<bool> menu_open_{false};
    std::thread worker_{};
};

