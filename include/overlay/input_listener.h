#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

struct TouchEvent {
    int x{0};
    int y{0};
    bool down{false};
    bool up{false};
    bool move{false};
    bool tap{false};
};

class InputListener {
public:
    using Callback = std::function<void(const TouchEvent &)>;

    bool start(int screen_w, int screen_h, Callback cb);
    void stop();

private:
    void worker();
    void enumerate_devices();
    void handle_event(int fd);
    std::optional<std::pair<int, int>> normalize_xy(int raw_x, int raw_y) const;

    struct DeviceInfo {
        int fd{-1};
        int min_x{0};
        int max_x{4096};
        int min_y{0};
        int max_y{4096};
        std::string name;
    };

    int screen_w_{1080};
    int screen_h_{2400};
    Callback callback_{};
    std::vector<DeviceInfo> devices_;
    bool running_{false};
    bool finger_down_{false};
    int last_x_{0};
    int last_y_{0};
    int start_x_{0};
    int start_y_{0};
    int move_threshold_{18};
};

