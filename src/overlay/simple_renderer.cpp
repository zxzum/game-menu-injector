#include "overlay/simple_renderer.h"

#include <android/log.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <thread>
#include <vector>

namespace {
    constexpr const char *kLogTag = "Overlay";

    // 5x7 font for A-Z, _, numbers and a few symbols (generated manually, minimal set for logging).
    const std::array<std::array<uint8_t, 7>, 36 + 2> kFont = [] {
        std::array<std::array<uint8_t, 7>, 36 + 2> font{};
        auto set = [&font](char c, std::array<uint8_t, 7> rows) {
            size_t idx;
            if (c >= 'A' && c <= 'Z') idx = c - 'A';
            else if (c >= '0' && c <= '9') idx = 26 + (c - '0');
            else if (c == '_') idx = 36;
            else idx = 37; // space
            font[idx] = rows;
        };

        set('A', {0x1E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11});
        set('B', {0x1E, 0x11, 0x1E, 0x11, 0x11, 0x11, 0x1E});
        set('C', {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E});
        set('D', {0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C});
        set('E', {0x1F, 0x10, 0x1E, 0x10, 0x10, 0x10, 0x1F});
        set('F', {0x1F, 0x10, 0x1E, 0x10, 0x10, 0x10, 0x10});
        set('G', {0x0E, 0x11, 0x10, 0x13, 0x11, 0x11, 0x0E});
        set('H', {0x11, 0x11, 0x1F, 0x11, 0x11, 0x11, 0x11});
        set('I', {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E});
        set('J', {0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0C});
        set('K', {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11});
        set('L', {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F});
        set('M', {0x11, 0x1B, 0x15, 0x11, 0x11, 0x11, 0x11});
        set('N', {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11});
        set('O', {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E});
        set('P', {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10});
        set('Q', {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D});
        set('R', {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11});
        set('S', {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E});
        set('T', {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04});
        set('U', {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E});
        set('V', {0x11, 0x11, 0x11, 0x0A, 0x0A, 0x04, 0x04});
        set('W', {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11});
        set('X', {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11});
        set('Y', {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04});
        set('Z', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F});

        set('0', {0x0E, 0x13, 0x15, 0x15, 0x15, 0x19, 0x0E});
        set('1', {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E});
        set('2', {0x0E, 0x11, 0x01, 0x06, 0x08, 0x10, 0x1F});
        set('3', {0x1F, 0x01, 0x02, 0x06, 0x01, 0x11, 0x0E});
        set('4', {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02});
        set('5', {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E});
        set('6', {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E});
        set('7', {0x1F, 0x01, 0x02, 0x04, 0x04, 0x04, 0x04});
        set('8', {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E});
        set('9', {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C});

        set('_', {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F});
        set(' ', {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
        return font;
    }();
}

bool SimpleRenderer::initialize() {
    fd_ = open("/dev/graphics/fb0", O_RDWR);
    if (fd_ < 0) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "Failed to open framebuffer: %s", strerror(errno));
        return false;
    }

    fb_fix_screeninfo finfo{};
    fb_var_screeninfo vinfo{};

    if (ioctl(fd_, FBIOGET_FSCREENINFO, &finfo) < 0 || ioctl(fd_, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "Failed to read fb info: %s", strerror(errno));
        close(fd_);
        fd_ = -1;
        return false;
    }

    width_ = vinfo.xres;
    height_ = vinfo.yres;
    bpp_ = vinfo.bits_per_pixel;
    line_length_ = finfo.line_length;
    buffer_size_ = finfo.smem_len;

    fb_map_ = static_cast<uint8_t *>(mmap(nullptr, buffer_size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0));
    if (fb_map_ == MAP_FAILED) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "Failed to mmap framebuffer: %s", strerror(errno));
        close(fd_);
        fd_ = -1;
        return false;
    }
    mapped_ = true;
    __android_log_print(ANDROID_LOG_INFO, kLogTag, "Framebuffer mapped: %dx%d bpp=%d line_len=%zu size=%zu", width_, height_, bpp_, line_length_, buffer_size_);
    return true;
}

void SimpleRenderer::render_frame(const OverlayFrame &frame) {
    if (!mapped_ || !fb_map_) {
        return;
    }

    // Copy current frame to a scratch buffer so we don't permanently overwrite the display when moving the menu.
    static std::vector<uint8_t> scratch;
    if (scratch.size() != buffer_size_) {
        scratch.resize(buffer_size_);
    }
    std::memcpy(scratch.data(), fb_map_, buffer_size_);

    // Draw loader square.
    draw_rect(scratch.data(), frame.surface_width - frame.menu_w - 20, 20, frame.menu_w, frame.menu_h, 0xFF2ECC71, 180);
    draw_text(scratch.data(), frame.surface_width - frame.menu_w - 12, 28, "MENU_LOADER", 0xFF0B0C10, 255);

    if (frame.menu_open) {
        draw_rect(scratch.data(), frame.menu_x, frame.menu_y, frame.menu_w, frame.menu_h, frame.dragging ? 0xFF3498DB : 0xFF9B59B6, 210);
        draw_text(scratch.data(), frame.menu_x + 12, frame.menu_y + 12, "MENU_LOADER", 0xFFFFFFFF, 255);
        draw_text(scratch.data(), frame.menu_x + 12, frame.menu_y + 36, "DRAG TO MOVE", 0xFFECECEC, 230);
        draw_text(scratch.data(), frame.menu_x + 12, frame.menu_y + 56, "TAP LABEL TO CLOSE", 0xFFECECEC, 230);
    }

    std::memcpy(fb_map_, scratch.data(), buffer_size_);
}

void SimpleRenderer::shutdown() {
    if (mapped_ && fb_map_) {
        munmap(fb_map_, buffer_size_);
        fb_map_ = nullptr;
    }
    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }
    mapped_ = false;
    __android_log_print(ANDROID_LOG_INFO, kLogTag, "Renderer shutdown");
}

uint32_t SimpleRenderer::read_pixel(const uint8_t *pixel_ptr) const {
    if (bpp_ == 32) {
        return *reinterpret_cast<const uint32_t *>(pixel_ptr);
    }
    if (bpp_ == 16) {
        uint16_t v = *reinterpret_cast<const uint16_t *>(pixel_ptr);
        uint8_t r = ((v >> 11) & 0x1F) << 3;
        uint8_t g = ((v >> 5) & 0x3F) << 2;
        uint8_t b = (v & 0x1F) << 3;
        return (0xFF << 24) | (r << 16) | (g << 8) | b;
    }
    return 0;
}

void SimpleRenderer::write_pixel(uint8_t *pixel_ptr, uint32_t color) {
    if (bpp_ == 32) {
        *reinterpret_cast<uint32_t *>(pixel_ptr) = color;
    } else if (bpp_ == 16) {
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = color & 0xFF;
        uint16_t v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        *reinterpret_cast<uint16_t *>(pixel_ptr) = v;
    }
}

void SimpleRenderer::blend_pixel(uint8_t *pixel_ptr, uint32_t color, uint8_t alpha) {
    uint32_t dst = read_pixel(pixel_ptr);
    uint8_t dst_r = (dst >> 16) & 0xFF;
    uint8_t dst_g = (dst >> 8) & 0xFF;
    uint8_t dst_b = dst & 0xFF;

    uint8_t src_r = (color >> 16) & 0xFF;
    uint8_t src_g = (color >> 8) & 0xFF;
    uint8_t src_b = color & 0xFF;

    auto blend = [alpha](uint8_t s, uint8_t d) {
        return static_cast<uint8_t>((s * alpha + d * (255 - alpha)) / 255);
    };

    uint32_t out = (0xFF << 24) | (blend(src_r, dst_r) << 16) | (blend(src_g, dst_g) << 8) | blend(src_b, dst_b);
    write_pixel(pixel_ptr, out);
}

void SimpleRenderer::draw_rect(uint8_t *buffer, int x, int y, int w, int h, uint32_t color, uint8_t alpha) {
    int start_x = std::max(0, x);
    int start_y = std::max(0, y);
    int end_x = std::min(width_, x + w);
    int end_y = std::min(height_, y + h);

    for (int j = start_y; j < end_y; ++j) {
        for (int i = start_x; i < end_x; ++i) {
            uint8_t *pix = buffer + j * line_length_ + i * (bpp_ / 8);
            blend_pixel(pix, color, alpha);
        }
    }
}

void SimpleRenderer::draw_text(uint8_t *buffer, int x, int y, const std::string &text, uint32_t color, uint8_t alpha) {
    int cursor_x = x;
    for (char ch : text) {
        size_t idx;
        if (ch >= 'A' && ch <= 'Z') idx = ch - 'A';
        else if (ch >= '0' && ch <= '9') idx = 26 + (ch - '0');
        else if (ch == '_') idx = 36;
        else idx = 37;

        const auto &glyph = kFont[idx];
        for (int row = 0; row < 7; ++row) {
            uint8_t bits = glyph[row];
            for (int col = 0; col < 5; ++col) {
                if (bits & (1 << (4 - col))) {
                    uint8_t *pix = buffer + (y + row) * line_length_ + (cursor_x + col) * (bpp_ / 8);
                    blend_pixel(pix, color, alpha);
                }
            }
        }
        cursor_x += 6; // 5px + 1px spacing
    }
}

