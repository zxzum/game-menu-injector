#pragma once

#include <optional>
#include <string>
#include <sys/types.h>

class ProcessManager {
public:
    explicit ProcessManager(std::string package_name);

    bool initialize();
    bool launch_game();
    std::optional<pid_t> pid() const;

private:
    std::optional<pid_t> find_process() const;
    bool run_shell_command(const std::string &cmd) const;

    std::string package_name_;
    std::optional<pid_t> pid_{};
};

