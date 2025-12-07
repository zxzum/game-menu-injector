#include "overlay/input_listener.h"

#include <android/log.h>
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <thread>

namespace {
    constexpr const char *kLogTag = "Input";
    const char *kInputDir = "/dev/input";
}

bool InputListener::start(int screen_w, int screen_h, Callback cb) {
    if (running_) {
        return true;
    }

    screen_w_ = screen_w;
    screen_h_ = screen_h;
    callback_ = std::move(cb);
    enumerate_devices();

    if (devices_.empty()) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "No touch-capable /dev/input devices found");
        return false;
    }

    running_ = true;
    std::thread(&InputListener::worker, this).detach();
    __android_log_print(ANDROID_LOG_INFO, kLogTag, "Input listener started for %d devices", static_cast<int>(devices_.size()));
    return true;
}

void InputListener::stop() {
    running_ = false;
    for (auto &dev : devices_) {
        if (dev.fd >= 0) {
            close(dev.fd);
            dev.fd = -1;
        }
    }
    devices_.clear();
    __android_log_print(ANDROID_LOG_INFO, kLogTag, "Input listener stopped");
}

void InputListener::enumerate_devices() {
    devices_.clear();
    DIR *dir = opendir(kInputDir);
    if (!dir) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "Failed to open %s", kInputDir);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (std::strncmp(entry->d_name, "event", 5) != 0) {
            continue;
        }

        std::string path = std::string(kInputDir) + "/" + entry->d_name;
        int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
        if (fd < 0) {
            __android_log_print(ANDROID_LOG_WARN, kLogTag, "Failed to open %s", path.c_str());
            continue;
        }

        char name[256] = {0};
        ioctl(fd, EVIOCGNAME(sizeof(name)), name);

        unsigned long ev_bits[(EV_MAX + 7) / 8] = {0};
        if (ioctl(fd, EVIOCGBIT(0, sizeof(ev_bits)), ev_bits) < 0) {
            close(fd);
            continue;
        }

        bool has_abs = ev_bits[EV_ABS / (sizeof(unsigned long) * 8)] & (1UL << (EV_ABS % (sizeof(unsigned long) * 8)));
        if (!has_abs) {
            close(fd);
            continue;
        }

        input_absinfo abs_x{};
        input_absinfo abs_y{};
        if (ioctl(fd, EVIOCGABS(ABS_MT_POSITION_X), &abs_x) < 0 || ioctl(fd, EVIOCGABS(ABS_MT_POSITION_Y), &abs_y) < 0) {
            close(fd);
            continue;
        }

        DeviceInfo info;
        info.fd = fd;
        info.min_x = abs_x.minimum;
        info.max_x = abs_x.maximum;
        info.min_y = abs_y.minimum;
        info.max_y = abs_y.maximum;
        info.name = name;
        devices_.push_back(info);

        __android_log_print(ANDROID_LOG_INFO, kLogTag, "Using input %s (name=%s) with ranges X[%d,%d] Y[%d,%d]", path.c_str(), name, info.min_x, info.max_x, info.min_y, info.max_y);
    }

    closedir(dir);
}

void InputListener::worker() {
    constexpr int kPollTimeoutMs = 50;
    while (running_) {
        std::vector<pollfd> fds;
        fds.reserve(devices_.size());
        for (auto &dev : devices_) {
            if (dev.fd >= 0) {
                pollfd p{};
                p.fd = dev.fd;
                p.events = POLLIN;
                fds.push_back(p);
            }
        }

        if (fds.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }

        int ret = poll(fds.data(), fds.size(), kPollTimeoutMs);
        if (ret <= 0) {
            continue;
        }

        for (size_t i = 0; i < fds.size(); ++i) {
            if (fds[i].revents & POLLIN) {
                handle_event(fds[i].fd);
            }
        }
    }
}

void InputListener::handle_event(int fd) {
    struct input_event events[32];
    ssize_t rd = read(fd, events, sizeof(events));
    if (rd <= 0) {
        return;
    }

    int count = rd / sizeof(struct input_event);
    for (int i = 0; i < count; ++i) {
        auto &ev = events[i];
        if (ev.type == EV_ABS) {
            if (ev.code == ABS_MT_POSITION_X) {
                last_x_ = ev.value;
            } else if (ev.code == ABS_MT_POSITION_Y) {
                last_y_ = ev.value;
            }
        } else if (ev.type == EV_KEY && ev.code == BTN_TOUCH) {
            if (ev.value == 1) {
                finger_down_ = true;
                start_x_ = last_x_;
                start_y_ = last_y_;
                auto norm = normalize_xy(last_x_, last_y_);
                if (norm.has_value()) {
                    TouchEvent te{};
                    te.x = norm->first;
                    te.y = norm->second;
                    te.down = true;
                    if (callback_) callback_(te);
                }
            } else if (ev.value == 0) {
                bool was_down = finger_down_;
                finger_down_ = false;
                auto norm = normalize_xy(last_x_, last_y_);
                if (!norm.has_value()) {
                    continue;
                }
                TouchEvent te{};
                te.x = norm->first;
                te.y = norm->second;
                te.up = true;

                int dx = std::abs(last_x_ - start_x_);
                int dy = std::abs(last_y_ - start_y_);
                if (was_down && dx < move_threshold_ && dy < move_threshold_) {
                    te.tap = true;
                }
                if (callback_) callback_(te);
            }
        } else if (ev.type == EV_SYN && ev.code == SYN_REPORT) {
            if (finger_down_) {
                auto norm = normalize_xy(last_x_, last_y_);
                if (!norm.has_value()) continue;
                TouchEvent te{};
                te.x = norm->first;
                te.y = norm->second;
                te.move = true;
                if (callback_) callback_(te);
            }
        }
    }
}

std::optional<std::pair<int, int>> InputListener::normalize_xy(int raw_x, int raw_y) const {
    if (devices_.empty()) return std::nullopt;

    // Use first device ranges as baseline.
    const auto &d = devices_.front();
    int span_x = std::max(1, d.max_x - d.min_x);
    int span_y = std::max(1, d.max_y - d.min_y);

    int x = static_cast<int>((static_cast<double>(raw_x - d.min_x) / span_x) * screen_w_);
    int y = static_cast<int>((static_cast<double>(raw_y - d.min_y) / span_y) * screen_h_);

    x = std::clamp(x, 0, screen_w_ - 1);
    y = std::clamp(y, 0, screen_h_ - 1);
    return std::make_pair(x, y);
}

