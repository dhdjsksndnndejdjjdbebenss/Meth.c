cmake_minimum_required(VERSION 3.18.1)
project(meth CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.1
)
FetchContent_MakeAvailable(imgui)
add_subdirectory(dobby)

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

# THIS IS THE FIX: Add -DIMGUI_IMPL_OPENGL_ES2 so it knows it's on a phone
target_compile_options(meth PRIVATE -s -fvisibility=hidden -DIMGUI_IMPL_OPENGL_ES2)

target_link_libraries(meth dobby log android dl EGL GLESv2)
set_target_properties(meth PROPERTIES PREFIX "lib")
