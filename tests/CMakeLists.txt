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

add_executable(galaxian_tests
    test_timestep.cpp
    test_game_clock.cpp
)
target_link_libraries(galaxian_tests PRIVATE
    galaxian_core
    Catch2::Catch2WithMain
)

include(CTest)
catch_discover_tests(galaxian_tests)
