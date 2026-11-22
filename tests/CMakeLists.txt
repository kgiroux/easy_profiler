# Unit tests for easy_profiler core helpers (GoogleTest).
# Enabled with -DEASY_PROFILER_BUILD_TESTS=ON

# The root project accepts CMake >= 3.5, but the test setup needs newer features:
# cxx_std_20 (3.12), FetchContent_MakeAvailable (3.14), gtest_discover_tests (3.10).
if (CMAKE_VERSION VERSION_LESS "3.14")
    message(FATAL_ERROR "EASY_PROFILER_BUILD_TESTS requires CMake >= 3.14 (found ${CMAKE_VERSION})")
endif ()

find_package(GTest CONFIG QUIET)
if (NOT GTest_FOUND)
    include(FetchContent)
    set(_easy_gtest_fetch_options)
    if (NOT CMAKE_VERSION VERSION_LESS "3.24")
        # Keyword only known since CMake 3.24 (older versions would treat it as an extra URL)
        list(APPEND _easy_gtest_fetch_options DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    endif ()
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
        ${_easy_gtest_fetch_options}
    )
    # Use the same CRT as the parent project on MSVC
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif ()

add_executable(easy_profiler_tests
    tick_conversion_test.cpp
    min_scale_test.cpp
    reader_robustness_test.cpp
    block_budget_test.cpp
)

# Tests are written in C++20; the library itself stays C++11-compatible.
target_compile_features(easy_profiler_tests PRIVATE cxx_std_20)

target_include_directories(easy_profiler_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/easy_profiler_core           # private headers (tick_conversion.h)
    ${PROJECT_SOURCE_DIR}/easy_profiler_core/include   # public headers
    ${PROJECT_SOURCE_DIR}/profiler_gui                 # Qt-free GUI helpers (min_scale.h)
)

target_link_libraries(easy_profiler_tests PRIVATE easy_profiler GTest::gtest_main $<BUILD_INTERFACE:easy_profiler_sanitizers>)

include(GoogleTest)
gtest_discover_tests(easy_profiler_tests)
