# Compiler warning and optimization policy.
# Policy: zero warnings at -Wall -Wextra -Wpedantic (see docs/test_plan.md).

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O1)
    elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O2)
    elseif(CMAKE_BUILD_TYPE STREQUAL "Sanitize")
        add_compile_options(-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    endif()
endif()
