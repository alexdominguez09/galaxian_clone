cmake_minimum_required(VERSION 3.20)

project(galaxian
    VERSION 0.1.0
    DESCRIPTION "Galaxian-style arcade shooter (original assets)"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to a Debug build for single-config generators unless the user chose one.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release Sanitize)
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    enable_testing()
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerFlags)

find_package(SDL2 REQUIRED)

add_executable(galaxian
    src/main.cpp
    src/Game.cpp
)

target_include_directories(galaxian PRIVATE src)
target_link_libraries(galaxian PRIVATE SDL2::SDL2)

add_subdirectory(tests)
