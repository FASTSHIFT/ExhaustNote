cmake_minimum_required(VERSION 3.16)
project(ExhaustNote VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(TARGET_PLATFORM "Target platform: sim or mcu" "sim")
option(ENABLE_COVERAGE "Enable code coverage instrumentation" OFF)

if(ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(--coverage -fprofile-arcs -ftest-coverage)
        add_link_options(--coverage)
    endif()
endif()

add_subdirectory(core)

if(TARGET_PLATFORM STREQUAL "sim")
    add_subdirectory(platform/sim)
    add_subdirectory(app/sim)
    add_subdirectory(app/sim_gui)
    enable_testing()
    add_subdirectory(tests)
endif()
