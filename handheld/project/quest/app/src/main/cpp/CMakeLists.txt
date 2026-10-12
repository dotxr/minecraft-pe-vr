# Native build for the Quest (arm64-v8a, OpenXR) port of Minecraft PE 0.6.1.
cmake_minimum_required(VERSION 3.18)
project(minecraftpe_quest CXX C)

# The engine code predates C++11; build it in the same dialect it was written for.
set(CMAKE_CXX_STANDARD 98)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

set(HANDHELD "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../..")
get_filename_component(HANDHELD "${HANDHELD}" ABSOLUTE)

include(${CMAKE_CURRENT_SOURCE_DIR}/game_sources.cmake)

# The OpenXR and GLES 3 layer is written against C++17 (designated-initialiser style
# braces); the engine stays on gnu++98. Separate targets keep the two dialects apart.
add_library(vrlayer STATIC
    ${HANDHELD}/src/platform/vr/vr_state.cpp
    ${HANDHELD}/src/platform/vr/vr_render.cpp
    ${HANDHELD}/src/platform/vr/xr_session.cpp
)
set_target_properties(vrlayer PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS ON)
target_compile_options(vrlayer PRIVATE -Wno-psabi)
target_include_directories(vrlayer PRIVATE ${HANDHELD}/src)
target_link_libraries(vrlayer PRIVATE openxr_loader)

# Khronos OpenXR loader and headers. Pinned to the commit this port was built and
# checked against; the loader is built for the same NDK as the game.
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
# Put libopenxr_loader.so next to the game library so the APK packages it.
set_target_properties(openxr_loader PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")


add_library(raknet STATIC ${RAKNET_SOURCES})
target_include_directories(raknet PRIVATE ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources)

# NDK activity glue (NativeActivity event loop helpers)
add_library(native_app_glue STATIC ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)
target_include_directories(native_app_glue PUBLIC ${ANDROID_NDK}/sources/android/native_app_glue)

add_library(minecraftpe SHARED ${GAME_SOURCES})
target_include_directories(minecraftpe PRIVATE ${HANDHELD}/src ${HANDHELD}/src/raknet)
target_compile_definitions(minecraftpe PRIVATE ANDROID MCPE_VR=1)
target_compile_options(minecraftpe PRIVATE -Wno-psabi -fpermissive -Wno-deprecated-declarations -Wno-invalid-offsetof -Wno-error=format-security)
# The glue provides ANativeActivity_onCreate, which NativeActivity looks up in the
# library. Linked as a static archive the linker drops it (nothing references it), so
# the game compiles the glue source in directly.
target_sources(minecraftpe PRIVATE ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)
target_include_directories(minecraftpe PRIVATE ${ANDROID_NDK}/sources/android/native_app_glue)
target_link_libraries(minecraftpe PRIVATE vrlayer raknet openxr_loader log android EGL GLESv3 OpenSLES)
