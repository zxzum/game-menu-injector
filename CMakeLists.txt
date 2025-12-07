# CMakeLists.txt for Android NDK Project

cmake_minimum_required(VERSION 3.10)
project(game-menu-injector)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SRC_FILES
    src/main.cpp
    src/process/process_manager.cpp
    src/overlay/menu_overlay.cpp
    src/overlay/simple_renderer.cpp
    src/injector/kitty_memory_bridge.cpp
)

add_executable(game-menu-injector ${SRC_FILES})

target_include_directories(game-menu-injector PRIVATE include)

find_library(log-lib log)

if(log-lib)
    target_link_libraries(game-menu-injector ${log-lib})
endif()

