cmake_minimum_required(VERSION 3.18.1)
project(meth CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG docking 
)
FetchContent_MakeAvailable(imgui)
add_subdirectory(dobby)

# This time we are explicitly adding the ImGui source files
add_library(meth SHARED 
    meth.cpp
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_android.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(meth PRIVATE ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(meth dobby log android dl EGL GLESv2)
target_compile_options(meth PRIVATE -s -fvisibility=hidden)
set_target_properties(meth PROPERTIES PREFIX "lib")
