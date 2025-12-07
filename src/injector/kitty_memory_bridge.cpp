#include "injector/kitty_memory_bridge.h"

#include <android/log.h>

namespace {
    constexpr const char *kLogTag = "KittyMemory";
}

bool KittyMemoryBridge::attach(pid_t pid) {
    target_pid_ = pid;
    __android_log_print(ANDROID_LOG_INFO, kLogTag, "Attached to pid %d using KittyMemory", pid);
    return true;
}

bool KittyMemoryBridge::write_title_string(const std::string &text_address_symbol, const std::string &value) {
    if (!target_pid_) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "attach() must be called before patching");
        return false;
    }

    __android_log_print(ANDROID_LOG_INFO, kLogTag,
                        "Would patch symbol %s with text '%s' (demo stub)",
                        text_address_symbol.c_str(), value.c_str());
    return true;
}

