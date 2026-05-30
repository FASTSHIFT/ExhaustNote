add_executable(exhaust_sim main.cpp)

target_link_libraries(exhaust_sim PRIVATE
    exhaust_core
    exhaust_platform_sim
)
