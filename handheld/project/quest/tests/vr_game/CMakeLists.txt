# Desktop build of the real game with the VR layer, run against an OpenXR runtime
# (Monado, headless). Compiles the same engine sources as the Quest build.
cmake_minimum_required(VERSION 3.18)
project(vr_game_host CXX C)

get_filename_component(HANDHELD "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
set(QUEST_CPP "${HANDHELD}/project/quest/app/src/main/cpp")
set(TESTS_STUBS "${HANDHELD}/project/quest/tests/gles1_shim/stub")

include(${QUEST_CPP}/game_sources.cmake)

# The Android entry point and OpenSL sound are not used on the desktop.
list(FILTER GAME_SOURCES EXCLUDE REGEX "main_quest\\.cpp$")
list(FILTER GAME_SOURCES EXCLUDE REGEX "SoundSystemSL\\.cpp$")

# Khronos OpenXR loader and headers, built for the host from the same pinned commit.
include(FetchContent)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_CONFORMANCE_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_API_LAYERS OFF CACHE BOOL "" FORCE)
set(DYNAMIC_LOADER ON CACHE BOOL "" FORCE)
FetchContent_Declare(openxr_sdk
    GIT_REPOSITORY https://github.com/KhronosGroup/OpenXR-SDK.git
    GIT_TAG f2448a8797c85814aa892efc1ab8707900fbcc78
    GIT_SHALLOW FALSE)
FetchContent_MakeAvailable(openxr_sdk)

# The engine stays on gnu++98, as in the Quest build. Set per target so the OpenXR
# subproject above keeps the compiler's default dialect.
add_library(raknet STATIC ${RAKNET_SOURCES})
target_include_directories(raknet PRIVATE ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources)
target_compile_options(raknet PRIVATE -w)
set_target_properties(raknet PROPERTIES CXX_STANDARD 98 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS ON)

# The VR layer, in the same C++17 dialect the Quest build uses.
add_library(vrlayer STATIC
    ${HANDHELD}/src/platform/vr/vr_state.cpp
    ${HANDHELD}/src/platform/vr/vr_render.cpp
    ${HANDHELD}/src/platform/vr/xr_session.cpp
)
set_target_properties(vrlayer PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS ON)
target_compile_definitions(vrlayer PRIVATE XR_HOST_TEST)
target_compile_options(vrlayer PRIVATE -w)
target_include_directories(vrlayer PRIVATE ${HANDHELD}/src ${TESTS_STUBS})
target_link_libraries(vrlayer PRIVATE openxr_loader)

# The game. OPENGL_ES selects the GLES 1 paths; MCPE_VR selects the headset paths.
add_executable(vr_game ${GAME_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/host_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/host_platform.cpp)
target_include_directories(vr_game PRIVATE ${HANDHELD}/src ${HANDHELD}/src/raknet
    ${TESTS_STUBS} ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(vr_game PRIVATE OPENGL_ES MCPE_VR=1 XR_HOST_TEST
    MCPE_ASSETS_DIR="${HANDHELD}/project/android/assets" MCPE_DATA_DIR="/tmp/vr-game-data")
target_compile_options(vr_game PRIVATE -w -fpermissive)
set_target_properties(vr_game PROPERTIES CXX_STANDARD 98 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS ON)
target_link_libraries(vr_game PRIVATE vrlayer raknet openxr_loader EGL GLESv2 pthread dl)
