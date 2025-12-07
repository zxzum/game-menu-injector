#include "overlay/simple_renderer.h"

#include <android/log.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>

namespace {
    constexpr const char *kLogTag = "Overlay";

    std::string build_ascii_box(const OverlayFrame &frame) {
        std::ostringstream out;
        out << "[" << frame.title << "]";
        if (frame.menu_open) {
            out << "\n" << "< MENU OPENED >";
        } else {
            out << "\n" << "< MENU HIDDEN >";
        }
        return out.str();
    }
}

bool SimpleRenderer::initialize() {
    __android_log_print(ANDROID_LOG_INFO, kLogTag, "Renderer bootstrap for virtual surface %dx%d", 1080, 2400);
    return true;
}

void SimpleRenderer::render_frame(const OverlayFrame &frame) {
    std::string ascii = build_ascii_box(frame);
    __android_log_print(ANDROID_LOG_INFO, kLogTag, "Drawing overlay:\n%s", ascii.c_str());
}

void SimpleRenderer::shutdown() {
    __android_log_print(ANDROID_LOG_INFO, kLogTag, "Renderer shutdown");
}

