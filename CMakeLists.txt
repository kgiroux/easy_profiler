cmake_minimum_required(VERSION 3.5...${CMAKE_VERSION})
project(easy_profiler CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if (CMAKE_VERSION VERSION_LESS "3.1")
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
      set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11")
    endif ()
else ()
    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif ()

if (MSVC)
    if (NOT (MSVC_VERSION LESS 1914))
        # turn on valid __cplusplus macro value for visual studio (available since msvc 2017 update 7)
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:__cplusplus")
    endif ()
endif ()

option(EASY_PROFILER_NO_GUI "Build easy_profiler without the GUI application (required Qt)" OFF)
option(EASY_PROFILER_ENABLE_ASAN "Instrument easy_profiler, the GUI and the tests with AddressSanitizer (development only)" OFF)

# Sanitizer flags are carried by an INTERFACE target linked explicitly by each target
# (wrapped in $<BUILD_INTERFACE:...> so that it never leaks into the installed package).
add_library(easy_profiler_sanitizers INTERFACE)
if (EASY_PROFILER_ENABLE_ASAN)
    if (CMAKE_VERSION VERSION_LESS "3.13")
        message(FATAL_ERROR "EASY_PROFILER_ENABLE_ASAN requires CMake >= 3.13")
    endif ()
    if (MSVC)
        target_compile_options(easy_profiler_sanitizers INTERFACE /fsanitize=address /Zi)
        target_link_options(easy_profiler_sanitizers INTERFACE /INCREMENTAL:NO /DEBUG)
    else ()
        target_compile_options(easy_profiler_sanitizers INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer -g)
        target_link_options(easy_profiler_sanitizers INTERFACE -fsanitize=address,undefined)
    endif ()
    message(STATUS "AddressSanitizer enabled")
endif ()

set(EASY_PROGRAM_VERSION_MAJOR 2)
set(EASY_PROGRAM_VERSION_MINOR 1)
set(EASY_PROGRAM_VERSION_PATCH 0)
set(EASY_PRODUCT_VERSION_STRING "${EASY_PROGRAM_VERSION_MAJOR}.${EASY_PROGRAM_VERSION_MINOR}.${EASY_PROGRAM_VERSION_PATCH}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)

# set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_LIST_DIR}/sdk)

macro(easy_define_target_option TARGET SOURCE_OPTION TARGET_DEFINITION)
    if (${SOURCE_OPTION})
        set(_VALUE 1)
    else ()
        set(_VALUE 0)
    endif ()
    target_compile_options(${TARGET} PUBLIC -D${TARGET_DEFINITION}=${_VALUE})
endmacro()

SET(CMAKE_INSTALL_RPATH "$ORIGIN")

add_subdirectory(easy_profiler_core)
if (NOT EASY_PROFILER_NO_GUI)
    add_subdirectory(profiler_gui)
endif()
add_subdirectory(easy_profiler_converter)

option(EASY_PROFILER_BUILD_TESTS "Build easy_profiler unit tests (GoogleTest)" OFF)
if (EASY_PROFILER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()

if (NOT EASY_PROFILER_NO_SAMPLES)
    add_subdirectory(sample)
    add_subdirectory(reader)
endif ()
