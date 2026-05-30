find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui)
set(IMPLOT_DIR ${CMAKE_SOURCE_DIR}/third_party/implot)

# ImGui sources (SDL2 + OpenGL3 backend)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# ImPlot sources
set(IMPLOT_SOURCES
    ${IMPLOT_DIR}/implot.cpp
    ${IMPLOT_DIR}/implot_items.cpp
)

add_executable(exhaust_sim_gui
    main.cpp
    wav_loader.cpp
    ${IMGUI_SOURCES}
    ${IMPLOT_SOURCES}
)

target_include_directories(exhaust_sim_gui PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${IMPLOT_DIR}
    ${CMAKE_SOURCE_DIR}/platform/include
)

target_link_libraries(exhaust_sim_gui PRIVATE
    exhaust_core
    SDL2::SDL2
    OpenGL::GL
    ${CMAKE_DL_LIBS}
)
