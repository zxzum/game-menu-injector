LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := game-menu-injector
LOCAL_CPPFLAGS  := -std=c++17
LOCAL_SRC_FILES := \
    ../src/main.cpp \
    ../src/process/process_manager.cpp \
    ../src/overlay/menu_overlay.cpp \
    ../src/overlay/input_listener.cpp \
    ../src/overlay/simple_renderer.cpp \
    ../src/injector/kitty_memory_bridge.cpp

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../include
LOCAL_LDLIBS    := -llog

include $(BUILD_EXECUTABLE)

