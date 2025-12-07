#include "process/process_manager.h"

#include <android/log.h>
#include <dirent.h>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <sys/system_properties.h>
#include <unistd.h>

namespace {
    constexpr const char *kLogTag = "Injector";

    std::string read_cmdline(const std::string &path) {
        std::ifstream file(path, std::ios::binary);
        std::string cmdline;
        std::getline(file, cmdline, '\0');
        return cmdline;
    }
}

ProcessManager::ProcessManager(std::string package_name)
    : package_name_(std::move(package_name)) {}

bool ProcessManager::run_shell_command(const std::string &cmd) const {
    int result = system(cmd.c_str());
    if (result != 0) {
        __android_log_print(ANDROID_LOG_WARN, kLogTag, "Command failed (%d): %s", result, cmd.c_str());
        return false;
    }
    return true;
}

std::optional<pid_t> ProcessManager::find_process() const {
    DIR *dir = opendir("/proc");
    if (!dir)
        return std::nullopt;

    dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type != DT_DIR)
            continue;

        std::string name(entry->d_name);
        if (name.find_first_not_of("0123456789") != std::string::npos)
            continue;

        std::string cmdline_path = "/proc/" + name + "/cmdline";
        std::string cmdline = read_cmdline(cmdline_path);
        if (cmdline == package_name_) {
            closedir(dir);
            return static_cast<pid_t>(std::stoi(name));
        }
    }

    closedir(dir);
    return std::nullopt;
}

bool ProcessManager::initialize() {
    pid_ = find_process();
    if (!pid_) {
        __android_log_print(ANDROID_LOG_INFO, kLogTag, "Process %s not running yet", package_name_.c_str());
        return false;
    }

    __android_log_print(ANDROID_LOG_INFO, kLogTag, "Found pid %d for %s", pid_.value(), package_name_.c_str());
    return true;
}

bool ProcessManager::launch_game() {
    std::ostringstream cmd;
    cmd << "am start -n " << package_name_ << "/.MainActivity";
    return run_shell_command(cmd.str());
}

std::optional<pid_t> ProcessManager::pid() const {
    return pid_;
}

