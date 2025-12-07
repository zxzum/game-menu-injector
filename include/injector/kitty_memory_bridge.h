#pragma once

#include <optional>
#include <string>
#include <sys/types.h>

class KittyMemoryBridge {
public:
    bool attach(pid_t pid);
    bool write_title_string(const std::string &text_address_symbol, const std::string &value);

private:
    std::optional<pid_t> target_pid_{};
};

