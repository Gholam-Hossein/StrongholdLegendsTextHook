cmake_minimum_required(VERSION 3.20)

project(StrongholdLegendsTextHook
    VERSION 0.1.0
    DESCRIPTION "Injectable text-render hook for Stronghold Legends"
    LANGUAGES CXX)

add_library(StrongholdLegendsTextHook SHARED
    src/main.cpp
    src/hook.cpp
    src/hook.h)

target_compile_features(StrongholdLegendsTextHook PRIVATE cxx_std_17)
target_compile_definitions(StrongholdLegendsTextHook PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Stronghold Legends is 32-bit. Configure this project with a Win32/x86 generator.")
endif()

set(MINHOOK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/minhook" CACHE PATH "Path to MinHook")

if(EXISTS "${MINHOOK_DIR}/CMakeLists.txt")
    add_subdirectory("${MINHOOK_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/minhook")
    target_link_libraries(StrongholdLegendsTextHook PRIVATE minhook)
else()
    message(FATAL_ERROR "MinHook is missing. Clone it into third_party/minhook before configuring.")
endif()

target_include_directories(StrongholdLegendsTextHook PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${MINHOOK_DIR}/include")

target_link_libraries(StrongholdLegendsTextHook PRIVATE user32)
