#include "overlay/menu_overlay.h"

#include <android/log.h>
#include <chrono>
#include <algorithm>
#include <mutex>
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

    bool ok = renderer_.initialize();
    if (!ok) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "Renderer init failed");
        return false;
    }

    screen_w_ = renderer_.width();
    screen_h_ = renderer_.height();

    menu_x_ = screen_w_ - menu_w_ - 28;
    menu_y_ = 28;

    if (!input_.start(screen_w_, screen_h_, [this](const TouchEvent &ev) { on_touch(ev); })) {
        __android_log_print(ANDROID_LOG_WARN, kLogTag, "Input listener failed, overlay still continues");
    }

    __android_log_print(ANDROID_LOG_INFO, kLogTag, "Overlay initialized with screen %dx%d menu size %dx%d", screen_w_, screen_h_, menu_w_, menu_h_);
    return true;
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
        renderer_.render_frame(snapshot_frame());
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}

void MenuOverlay::stop() {
    if (!running_.exchange(false))
        return;

    if (worker_.joinable()) {
        worker_.join();
    }
    input_.stop();
    renderer_.shutdown();
    g_instance = nullptr;
}

OverlayFrame MenuOverlay::snapshot_frame() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    OverlayFrame frame{};
    frame.title = title_;
    frame.menu_open = menu_open_.load();
    frame.surface_width = screen_w_;
    frame.surface_height = screen_h_;
    frame.menu_w = menu_w_;
    frame.menu_h = menu_h_;
    frame.menu_x = menu_x_;
    frame.menu_y = menu_y_;
    frame.dragging = dragging_;
    return frame;
}

void MenuOverlay::on_touch(const TouchEvent &ev) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (ev.tap) {
        // Center tap opens menu.
        int cx = screen_w_ / 2;
        int cy = screen_h_ / 2;
        int center_box_w = screen_w_ / 3;
        int center_box_h = screen_h_ / 3;
        bool in_center = (ev.x > cx - center_box_w / 2 && ev.x < cx + center_box_w / 2 && ev.y > cy - center_box_h / 2 && ev.y < cy + center_box_h / 2);
        bool in_loader = ev.x >= menu_x_ && ev.x <= menu_x_ + menu_w_ && ev.y >= menu_y_ && ev.y <= menu_y_ + menu_h_;

        if (!menu_open_.load() && in_center) {
            menu_open_.store(true);
            menu_x_ = cx - menu_w_ / 2;
            menu_y_ = cy - menu_h_ / 2;
            __android_log_print(ANDROID_LOG_INFO, kLogTag, "Tap center -> open menu at %d,%d", menu_x_, menu_y_);
            return;
        }

        if (menu_open_.load() && in_loader) {
            menu_open_.store(false);
            dragging_ = false;
            menu_x_ = screen_w_ - menu_w_ - 28;
            menu_y_ = 28;
            __android_log_print(ANDROID_LOG_INFO, kLogTag, "Tap loader -> close menu, reset to top-right");
            return;
        }
    }

    if (ev.down) {
        bool in_loader = ev.x >= menu_x_ && ev.x <= menu_x_ + menu_w_ && ev.y >= menu_y_ && ev.y <= menu_y_ + menu_h_;
        if (menu_open_.load() && in_loader) {
            dragging_ = true;
            drag_offset_x_ = ev.x - menu_x_;
            drag_offset_y_ = ev.y - menu_y_;
            __android_log_print(ANDROID_LOG_INFO, kLogTag, "Start dragging at %d,%d offset %d,%d", ev.x, ev.y, drag_offset_x_, drag_offset_y_);
        }
    }

    if (ev.move && dragging_) {
        int new_x = ev.x - drag_offset_x_;
        int new_y = ev.y - drag_offset_y_;
        new_x = std::clamp(new_x, 0, screen_w_ - menu_w_);
        new_y = std::clamp(new_y, 0, screen_h_ - menu_h_);
        menu_x_ = new_x;
        menu_y_ = new_y;
        __android_log_print(ANDROID_LOG_DEBUG, kLogTag, "Dragging -> %d,%d", menu_x_, menu_y_);
    }

    if (ev.up && dragging_) {
        dragging_ = false;
        __android_log_print(ANDROID_LOG_INFO, kLogTag, "Stop dragging at %d,%d", menu_x_, menu_y_);
    }
}

