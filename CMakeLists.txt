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
find_package(SDL2_ttf REQUIRED)
find_package(SDL2_image REQUIRED)

# Core library: timing + shared types + rendering + states. Kept separate
# so unit tests can link the logic without the full application.
add_library(galaxian_core STATIC
    src/core/GameClock.cpp
    src/core/TimestepController.cpp
    src/core/Game.cpp
    src/states/GameState.cpp
    src/states/StateMachine.cpp
    src/graphics/Texture.cpp
    src/graphics/Renderer.cpp
    src/graphics/DebugOverlay.cpp
    src/graphics/DevArt.cpp
    src/graphics/DevScene.cpp
    src/input/InputManager.cpp
    src/states/GameState.cpp
    src/states/StateMachine.cpp
    src/gameplay/AttackDirector.cpp
    src/gameplay/Combat.cpp
    src/gameplay/DivePath.cpp
    src/gameplay/Effects.cpp
    src/gameplay/Enemy.cpp
    src/gameplay/EnemyFormation.cpp
    src/gameplay/Player.cpp
    src/gameplay/Projectile.cpp
    src/gameplay/ScoreManager.cpp
    src/gameplay/WaveManager.cpp
)
target_include_directories(galaxian_core PUBLIC src)
target_link_libraries(galaxian_core PUBLIC
    SDL2::SDL2
    SDL2_ttf::SDL2_ttf
    SDL2_image::SDL2_image
)

add_executable(galaxian
    src/main.cpp
)
target_link_libraries(galaxian PRIVATE galaxian_core)

add_subdirectory(tests)
