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
set(RNDR_BUILD_SAMPLES OFF)
SET(RNDR_HARDENING ${RENDERING_COOKBOOK_HARDENING})
set(RNDR_SHARED_LIBS OFF)
FetchContent_Declare(
        rndr
        GIT_REPOSITORY https://github.com/praetorian555/rndr
        GIT_TAG master
)
FetchContent_MakeAvailable(rndr)

# Setup ETC2COMP ######################################################################
FetchContent_Declare(
        etc2comp
        GIT_REPOSITORY https://github.com/google/etc2comp
        GIT_TAG 39422c1aa2f4889d636db5790af1d0be6ff3a226
)
FetchContent_Populate(etc2comp)
FetchContent_GetProperties(etc2comp SOURCE_DIR ETC2COMP_SOURCE_PATH)
file(GLOB_RECURSE ETC2COMP_SOURCES CONFIGURE_DEPENDS
        ${ETC2COMP_SOURCE_PATH}/EtcLib/Etc/*.h
        ${ETC2COMP_SOURCE_PATH}/EtcLib/EtcCodec/*.h
        ${ETC2COMP_SOURCE_PATH}/EtcLib/Etc/*.cpp
        ${ETC2COMP_SOURCE_PATH}/EtcLib/EtcCodec/*.cpp)
add_library(etc2comp ${ETC2COMP_SOURCES}
        ${ETC2COMP_SOURCE_PATH}/EtcTool/EtcFile.h
        ${ETC2COMP_SOURCE_PATH}/EtcTool/EtcFile.cpp
        ${ETC2COMP_SOURCE_PATH}/EtcTool/EtcFileHeader.h
        ${ETC2COMP_SOURCE_PATH}/EtcTool/EtcFileHeader.cpp)
target_include_directories(etc2comp PUBLIC ${ETC2COMP_SOURCE_PATH})
target_include_directories(etc2comp PUBLIC ${ETC2COMP_SOURCE_PATH}/EtcLib/Etc
        ${ETC2COMP_SOURCE_PATH}/EtcLib/EtcCodec
        ${ETC2COMP_SOURCE_PATH}/EtcTool)
target_compile_options(etc2comp PRIVATE /W1)

# Setup ASSIMP ######################################################################
include(cmake/cpm.cmake)
cpmaddpackage(
        NAME
        assimp
        GITHUB_REPOSITORY
        "assimp/assimp"
        VERSION
        5.4.2
        OPTIONS
        "ASSIMP_ASSAN OFF"
        "BUILD_SHARED_LIBS OFF"
        "ASSIMP_BUILD_ASSIMP_TOOLS OFF"
        "ASSIMP_BUILD_TESTS OFF"
        "ASSIMP_NO_EXPORT ON"
        "ASSIMP_INSTALL_PDB OFF"
        "ASSIMP_BUILD_ZLIB ON"
        "ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF"
        "ASSIMP_BUILD_OBJ_IMPORTER ON"
        "ASSIMP_BUILD_FBX_IMPORTER ON"
        "ASSIMP_BUILD_GLTF_IMPORTER ON"
        "ASSIMP_BUILD_COLLADA_IMPORTER ON"
)
SET_TARGET_PROPERTIES(assimp PROPERTIES FOLDER Extern)
SET_TARGET_PROPERTIES(zlibstatic PROPERTIES FOLDER Extern)
SET_TARGET_PROPERTIES(UpdateAssimpLibsDebugSymbolsAndDLLs PROPERTIES FOLDER Extern)

# Setup MESHOPTIMIZER ######################################################################
cpmaddpackage(
        NAME
        meshoptimizer
        GITHUB_REPOSITORY
        "zeux/meshoptimizer"
        GIT_TAG
        "v0.20"
        OPTIONS
        "MESHOPT_WERROR ON"
)
SET_TARGET_PROPERTIES(meshoptimizer PROPERTIES FOLDER Extern)

# Setup GLI ###############################################################################
FetchContent_Declare(
        gli
        GIT_REPOSITORY https://github.com/g-truc/gli
        GIT_TAG 30808550a20ca53a255e6e1e77070493eda7b736 # 0.8.2
)
FetchContent_Populate(gli)
FetchContent_GetProperties(gli SOURCE_DIR GLI_SOURCE_PATH)
add_library(gli INTERFACE)
target_include_directories(gli INTERFACE ${GLI_SOURCE_PATH})
target_include_directories(gli INTERFACE ${GLI_SOURCE_PATH}/external/glm)
set_target_properties(gli PROPERTIES FOLDER Extern)

set(SHARED_FILES
        shared/types.h
        shared/cube-map.cpp
        shared/cube-map.h
        shared/material.cpp
        shared/material.h
        shared/mesh.cpp
        shared/mesh.h
        shared/scene.cpp
        shared/scene.h
        shared/assimp-helpers.h
        shared/assimp-helpers.cpp
        shared/animation.h
        shared/stb_image_resize2.h
        shared/stb_image.h
        shared/stb_image_write.h
        shared/stb_image.cpp
        shared/vulkan/vulkan-graphics-context.hpp
        shared/vulkan/vulkan-graphics-context.cpp
        shared/vulkan/vulkan-device.hpp
        shared/vulkan/vulkan-device.cpp
        shared/vulkan/vulkan-swap-chain.hpp
        shared/vulkan/vulkan-swap-chain.cpp)
add_library(shared STATIC ${SHARED_FILES})
target_link_libraries(shared PRIVATE rencook_options rencook_warnings)
target_link_libraries(shared PUBLIC rndr imgui etc2comp assimp meshoptimizer gli $ENV{VK_SDK_PATH}/Lib/vulkan-1.lib)
target_include_directories(shared PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/shared)
target_include_directories(shared PUBLIC $ENV{VK_SDK_PATH}/Include)
target_compile_definitions(shared PUBLIC -DASSETS_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/assets/")

add_executable(01-triangle chapters/01-triangle/triangle.cpp)
target_link_libraries(01-triangle PRIVATE shared)
target_link_libraries(01-triangle PRIVATE rencook_options rencook_warnings)

add_executable(02-cube chapters/02-cube/cube.cpp)
target_link_libraries(02-cube PRIVATE shared)
target_link_libraries(02-cube PRIVATE rencook_options rencook_warnings)

add_executable(03-image chapters/03-image/image.cpp)
target_link_libraries(03-image PRIVATE shared)
target_link_libraries(03-image PRIVATE rencook_options rencook_warnings)

add_executable(04-assimp chapters/04-assimp/assimp.cpp)
target_link_libraries(04-assimp PRIVATE shared)
target_link_libraries(04-assimp PRIVATE rencook_options rencook_warnings)

add_executable(05-mesh-optimizer chapters/05-mesh-optimizer/mesh-optimizer.cpp)
target_link_libraries(05-mesh-optimizer PRIVATE shared)
target_link_libraries(05-mesh-optimizer PRIVATE rencook_options rencook_warnings)

add_executable(06-vertex-pulling chapters/06-vertex-pulling/vertex-pulling.cpp)
target_link_libraries(06-vertex-pulling PRIVATE shared)
target_link_libraries(06-vertex-pulling PRIVATE rencook_options rencook_warnings)

add_executable(07-cube-map chapters/07-cube-map/cube-map.cpp)
target_link_libraries(07-cube-map PRIVATE shared)
target_link_libraries(07-cube-map PRIVATE rencook_options rencook_warnings)

add_executable(08-converters chapters/08-converters/converters.cpp chapters/08-converters/file-dialog.cpp)
target_link_libraries(08-converters PRIVATE shared)
# Needed since warnings are in third party library
target_link_libraries(08-converters PRIVATE rencook_options)

add_executable(09-pbr-shading chapters/09-pbr-shading/pbr-shading.cpp)
target_link_libraries(09-pbr-shading PRIVATE shared)
# Needed since warnings are in third party library
target_link_libraries(09-pbr-shading PRIVATE rencook_options)

add_executable(10-scene-renderer chapters/10-scene-renderer/scene-renderer.cpp)
target_link_libraries(10-scene-renderer PRIVATE shared)
# Needed since warnings are in third party library
target_link_libraries(10-scene-renderer PRIVATE rencook_options)

add_executable(11-shadows chapters/11-shadows/shadows.cpp)
target_link_libraries(11-shadows PRIVATE shared)
target_link_libraries(11-shadows PRIVATE rencook_options rencook_warnings)

add_executable(xx-game-animation chapters/xx-game-animation/game-animation.cpp)
target_link_libraries(xx-game-animation PRIVATE shared)
target_link_libraries(xx-game-animation PRIVATE rencook_options rencook_warnings)

add_executable(xx-vk-triangle chapters/xx-vk-triangle/vk-triangle.cpp)
target_link_libraries(xx-vk-triangle PRIVATE shared)
target_link_libraries(xx-vk-triangle PRIVATE rencook_options rencook_warnings)
