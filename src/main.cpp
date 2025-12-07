#include "injector/kitty_memory_bridge.h"
#include "overlay/menu_overlay.h"
#include "process/process_manager.h"

#include <android/log.h>
#include <atomic>
#include <chrono>
#include <csignal>
#include <thread>

namespace {
    constexpr const char *kLogTag = "Injector";
    std::atomic<bool> g_run{true};

    void handle_stop(int) {
        g_run.store(false);
    }
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    ProcessManager proc_manager("com.example.testgame");
    const int max_attempts = 3;
    bool initialized = false;

    for (int attempt = 0; attempt < max_attempts; ++attempt) {
        if (proc_manager.initialize()) {
            initialized = true;
            break;
        }

        if (attempt < max_attempts - 1) {
            proc_manager.launch_game();
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }

    if (!initialized || !proc_manager.pid()) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "Failed to attach to game process");
        return -1;
    }

    KittyMemoryBridge memory;
    memory.attach(proc_manager.pid().value());
    memory.write_title_string("kOverlayTitle", "Game Overlay");

    MenuOverlay overlay("Game Overlay");
    if (!overlay.initialize()) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "Overlay init failed");
        return -1;
    }

    std::signal(SIGINT, handle_stop);
    overlay.run();

    while (g_run.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    overlay.stop();
    __android_log_print(ANDROID_LOG_INFO, kLogTag, "Injector shutdown");
    return 0;
}

