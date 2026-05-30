add_library(exhaust_core STATIC
    src/crossfade.cpp
    src/sample_player.cpp
    src/dsp.cpp
    src/mixer.cpp
    src/transmission.cpp
    src/engine_voice.cpp
    src/engine_effects.cpp
    src/car_config.cpp
    ${CMAKE_SOURCE_DIR}/third_party/cjson/cJSON.c
)

target_include_directories(exhaust_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party/cjson
)

target_compile_features(exhaust_core PUBLIC cxx_std_17)
