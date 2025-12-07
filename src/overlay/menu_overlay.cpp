#include "overlay/menu_overlay.h"

#include <android/log.h>
#include <chrono>
#include <thread>

namespace {
    constexpr const char *kLogTag = "Overlay";
    MenuOverlay *g_instance = nullptr;
}

MenuOverlay::MenuOverlay(std::string title) : title_(std::move(title)) {}

MenuOverlay::~MenuOverlay() {
    stop();
}

void MenuOverlay::handle_signal(int sig) {
    if (sig == SIGUSR1 && g_instance) {
        g_instance->request_toggle();
    }
}

bool MenuOverlay::initialize() {
    g_instance = this;

    struct sigaction action{};
    action.sa_handler = MenuOverlay::handle_signal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGUSR1, &action, nullptr);

    return renderer_.initialize();
}

void MenuOverlay::request_toggle() {
    bool current = menu_open_.load();
    menu_open_.store(!current);
    __android_log_print(ANDROID_LOG_INFO, kLogTag, "Menu toggle -> %s", menu_open_ ? "OPEN" : "CLOSED");
}

void MenuOverlay::run() {
    if (running_.exchange(true))
        return;

    worker_ = std::thread(&MenuOverlay::loop, this);
}

void MenuOverlay::loop() {
    while (running_.load()) {
        OverlayFrame frame{title_, menu_open_.load()};
        renderer_.render_frame(frame);
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}

void MenuOverlay::stop() {
    if (!running_.exchange(false))
        return;

    if (worker_.joinable()) {
        worker_.join();
    }
    renderer_.shutdown();
    g_instance = nullptr;
}

