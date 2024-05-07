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

set(SHARED_FILES shared/types.h shared/tmp.cpp)
add_library(shared ${SHARED_FILES})
target_link_libraries(shared PRIVATE rencook_options rencook_warnings)
target_link_libraries(shared PUBLIC rndr math opal)
target_include_directories(shared PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/shared)

add_executable(01-triangle chapters/01-triangle/triangle.cpp)
target_link_libraries(01-triangle PRIVATE shared)
target_link_libraries(01-triangle PRIVATE rencook_options rencook_warnings)

add_executable(02-cube chapters/02-cube/cube.cpp)
target_link_libraries(02-cube PRIVATE shared)
target_link_libraries(02-cube PRIVATE rencook_options rencook_warnings)
