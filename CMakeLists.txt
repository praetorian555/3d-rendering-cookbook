cmake_minimum_required(VERSION 3.28)
project(RENDERING_COOKBOOK VERSION 1.0.0 LANGUAGES CXX)

option(RENDERING_COOKBOOK_HARDENING "Enable hardening options" ON)
message(STATUS "RENDERING_COOKBOOK_HARDENING: ${RENDERING_COOKBOOK_HARDENING}")

# Configure desired compilation options and warnings
include(cmake/compiler-warnings.cmake)
include(cmake/compiler-options.cmake)
add_library(rencook_warnings INTERFACE)
add_library(rencook_options INTERFACE)
setup_compiler_warnings(rencook_warnings)
setup_compiler_options(rencook_options)

if (RENDERING_COOKBOOK_HARDENING)
    include(cmake/sanitizers.cmake)
    setup_sanitizers(rencook_options)
endif ()

include(FetchContent)
SET(RNDR_BUILD_TESTS OFF)
SET(RNDR_HARDENING ${RNDR_HARDENING})
set(RNDR_SHARED_LIBS OFF)
FetchContent_Declare(
        rndr
        GIT_REPOSITORY https://github.com/praetorian555/rndr
        GIT_TAG        master
)
FetchContent_MakeAvailable(rndr)

FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui
        GIT_TAG c6e0284ac58b3f205c95365478888f7b53b077e2 # release-1.89.9
)
FetchContent_MakeAvailable(imgui)
FetchContent_GetProperties(imgui SOURCE_DIR SOURCE_PATH)
set(IMGUI_SOURCE_FILES
        ${SOURCE_PATH}/imgui.cpp
        ${SOURCE_PATH}/imgui.h
        ${SOURCE_PATH}/imconfig.h
        ${SOURCE_PATH}/imgui_internal.h
        ${SOURCE_PATH}/imstb_rectpack.h
        ${SOURCE_PATH}/imstb_truetype.h
        ${SOURCE_PATH}/imstb_textedit.h
        ${SOURCE_PATH}/imgui_draw.cpp
        ${SOURCE_PATH}/imgui_demo.cpp
        ${SOURCE_PATH}/imgui_tables.cpp
        ${SOURCE_PATH}/imgui_widgets.cpp)
# Use Win32 backhand for Windows
if (MSVC)
    set(IMGUI_WINDOWS_SOURCE_FILES
            ${SOURCE_PATH}/backends/imgui_impl_win32.cpp
            ${SOURCE_PATH}/backends/imgui_impl_win32.h)
else ()
    set(IMGUI_WINDOWS_SOURCE_FILES)
endif ()

set(IMGUI_OPENGL_SOURCE_FILES
        ${SOURCE_PATH}/backends/imgui_impl_opengl3.cpp
        ${SOURCE_PATH}/backends/imgui_impl_opengl3.h)

add_library(imgui
        ${IMGUI_SOURCE_FILES}
        ${IMGUI_WINDOWS_SOURCE_FILES}
        ${IMGUI_OPENGL_SOURCE_FILES})
target_include_directories(imgui PUBLIC ${SOURCE_PATH})

set(SHARED_FILES shared/types.h shared/imgui-wrapper.cpp shared/imgui-wrapper.h)
add_library(shared ${SHARED_FILES})
target_link_libraries(shared PRIVATE rencook_options rencook_warnings)
target_link_libraries(shared PUBLIC rndr math opal imgui)
target_include_directories(shared PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/shared)
target_compile_definitions(shared PUBLIC -DASSETS_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/assets/")

add_executable(01-triangle chapters/01-triangle/triangle.cpp)
target_link_libraries(01-triangle PRIVATE shared)
target_link_libraries(01-triangle PRIVATE rencook_options rencook_warnings)

add_executable(02-cube chapters/02-cube/cube.cpp)
target_link_libraries(02-cube PRIVATE shared)
target_link_libraries(02-cube PRIVATE rencook_options rencook_warnings)