include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

# Unit tests
add_executable(exhaust_tests
    unit/test_crossfade.cpp
    unit/test_sample_player.cpp
    unit/test_dsp.cpp
    unit/test_mixer.cpp
    unit/test_transmission.cpp
    unit/test_engine_voice.cpp
)

target_link_libraries(exhaust_tests PRIVATE
    exhaust_core
    GTest::gtest_main
)

gtest_discover_tests(exhaust_tests)
