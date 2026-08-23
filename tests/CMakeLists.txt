# Unit tests: Catch2 v3, fetched at configure time (network required once;
# the source is cached in build/_deps afterwards).
include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(Catch2)

# Exposes catch_discover_tests() when Catch2 is pulled in via FetchContent.
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

# Catch2 v3.7.1's catch_matchers_string.cpp triggers a GCC 13
# -Wmaybe-uninitialized false positive under the Sanitize build
# (-O1 -fsanitize=address,undefined). It is third-party code, so suppress
# just that warning on the Catch2 targets instead of loosening the
# project-wide zero-warning policy (docs/test_plan.md §0).
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    foreach(_catch2_target IN ITEMS Catch2 Catch2WithMain)
        if(TARGET ${_catch2_target})
            target_compile_options(${_catch2_target} PRIVATE
                -Wno-maybe-uninitialized)
        endif()
    endforeach()
endif()

add_executable(galaxian_tests
    test_timestep.cpp
    test_game_clock.cpp
    test_types.cpp
    test_rendering.cpp
    test_input.cpp
    test_player.cpp
    test_player_damage.cpp
    test_projectiles.cpp
    test_collision.cpp
    test_attack_director.cpp
    test_animation.cpp
    test_dive_path.cpp
    test_enemy_fire.cpp
    test_enemy_formation.cpp
    test_enemy_state_machine.cpp
    test_formation_motion.cpp
    test_scoring.cpp
    test_combat.cpp
    test_game_states.cpp
    test_wave_manager.cpp
)
target_link_libraries(galaxian_tests PRIVATE
    galaxian_core
    Catch2::Catch2WithMain
)

include(CTest)
catch_discover_tests(galaxian_tests)

# Stage 4 dependency rule (docs/architecture.md §1, docs/test_plan.md):
# gameplay code must not reference SDL key constants. No-op until
# src/gameplay/ exists (created in Stage 5); fails if any SDLK_ appears.
add_test(
    NAME no_sdlk_in_gameplay
    COMMAND bash -c
            "if [ -d src/gameplay ]; then ! grep -rn 'SDLK_' src/gameplay; else true; fi"
)
set_tests_properties(no_sdlk_in_gameplay PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# Stage 7 dependency rule (docs/architecture.md §3.5): rendering code must
# not contain collision logic — the AABB rule lives in gameplay/Collision.hpp
# and graphics/ only draws the boxes it is given. Fails if the collision
# function name appears anywhere in src/graphics.
add_test(
    NAME no_collision_in_graphics
    COMMAND bash -c
            "if [ -d src/graphics ]; then ! grep -rn 'intersects' src/graphics; else true; fi"
)
set_tests_properties(no_collision_in_graphics PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
