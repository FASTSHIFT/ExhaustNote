find_package(SDL2 REQUIRED)

add_library(exhaust_platform_sim STATIC
    audio_output_sdl.cpp
    input_keyboard.cpp
)

target_include_directories(exhaust_platform_sim PUBLIC
    ${CMAKE_SOURCE_DIR}/platform/include
)

target_link_libraries(exhaust_platform_sim PUBLIC
    exhaust_core
    SDL2::SDL2
)
