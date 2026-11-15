cmake_minimum_required(VERSION 3.24)

# macOS: universal binary (arm64 + x86_64), macOS 11+.
if(APPLE)
    if(NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    endif()
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

# Windows: static MSVC runtime (/MT) for every target, including raylib and Jolt.
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "" FORCE)

project(LanternKnight LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(LK_DEBUG_TOOLS "Enable F1/F2/F5 debug tools (overlay, level editor, hot reload)" OFF)

include(cmake/Version.cmake)
include(cmake/Dependencies.cmake)

set(LK_BIN_DIR "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${LK_BIN_DIR}")
foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER ${cfg} CFG)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG} "${LK_BIN_DIR}")
endforeach()

function(lk_warnings tgt)
    if(MSVC)
        target_compile_options(${tgt} PRIVATE /W4 /permissive- /utf-8 /wd4244 /wd4267)
        target_compile_definitions(${tgt} PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
    else()
        target_compile_options(${tgt} PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
    endif()
endfunction()

function(lk_static_runtime tgt)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_options(${tgt} PRIVATE -static-libstdc++ -static-libgcc)
    endif()
endfunction()

# ---------------------------------------------------------------- lk_core ---
# Pure game rules: no raylib, no Jolt. Unit-tested.
file(GLOB_RECURSE LK_CORE_SRC CONFIGURE_DEPENDS src/core/*.cpp src/core/*.h)
add_library(lk_core STATIC ${LK_CORE_SRC})
target_include_directories(lk_core PUBLIC src)
target_link_libraries(lk_core PUBLIC nlohmann_json::nlohmann_json)
lk_warnings(lk_core)

# -------------------------------------------------------------- lk_engine ---
file(GLOB_RECURSE LK_ENGINE_SRC CONFIGURE_DEPENDS src/engine/*.cpp src/engine/*.h)
add_library(lk_engine STATIC ${LK_ENGINE_SRC})
target_include_directories(lk_engine PUBLIC src ${LK_CGLTF_INCLUDE_DIR})
target_link_libraries(lk_engine PUBLIC lk_core raylib Jolt nlohmann_json::nlohmann_json)
target_compile_definitions(lk_engine PUBLIC LK_VERSION="${LK_VERSION}"
    $<$<BOOL:${LK_DEBUG_TOOLS}>:LK_DEBUG_TOOLS=1>)
lk_warnings(lk_engine)

# ---------------------------------------------------------------- lk_game ---
file(GLOB_RECURSE LK_GAME_SRC CONFIGURE_DEPENDS src/game/*.cpp src/game/*.h)
add_library(lk_game STATIC ${LK_GAME_SRC})
target_link_libraries(lk_game PUBLIC lk_engine)
lk_warnings(lk_game)

# ------------------------------------------------------------ LanternKnight --
set(LK_EXE_SRC src/main.cpp)
if(WIN32)
    list(APPEND LK_EXE_SRC packaging/windows/LanternKnight.rc)
endif()
add_executable(LanternKnight ${LK_EXE_SRC})
target_link_libraries(LanternKnight PRIVATE lk_game)
lk_warnings(LanternKnight)
lk_static_runtime(LanternKnight)
if(WIN32 AND NOT LK_DEBUG_TOOLS)
    # GUI subsystem (no console window) but keep the standard main() entry point.
    set_target_properties(LanternKnight PROPERTIES WIN32_EXECUTABLE ON)
    if(MSVC)
        target_link_options(LanternKnight PRIVATE /ENTRY:mainCRTStartup)
    endif()
endif()

# -------------------------------------------------------------- tools -------
add_executable(asset_report tools/asset_report/main.cpp)
target_link_libraries(asset_report PRIVATE lk_engine)
lk_warnings(asset_report)
lk_static_runtime(asset_report)

# -------------------------------------------------------------- tests -------
enable_testing()
file(GLOB LK_TEST_SRC CONFIGURE_DEPENDS tests/*.cpp)
add_executable(lk_tests ${LK_TEST_SRC})
target_link_libraries(lk_tests PRIVATE lk_core doctest::doctest)
lk_warnings(lk_tests)
lk_static_runtime(lk_tests)
add_test(NAME unit COMMAND lk_tests)

include(cmake/Package.cmake)
