# CMakeLists.txt for Android NDK Project

cmake_minimum_required(VERSION 3.4.1)

# Set the project name
project(game-menu-injector)

# Specify the source files
set(SOURCE_FILES
    main.cpp
    # Add other source files here
)

# Add a library
add_library(game-menu-injector SHARED ${SOURCE_FILES})

# Include directories
include_directories(include)

# Find and link the necessary libraries
find_library(log-lib log)
target_link_libraries(game-menu-injector ${log-lib})