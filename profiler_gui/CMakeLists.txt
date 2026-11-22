#set(CMAKE_PREFIX_PATH f:/qt/5.5/5.6/msvc2013_64/lib/cmake)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Qt selection: EASY_PROFILER_QT_VERSION = 5 or 6 forces a major version,
# empty (default) auto-detects with Qt6 preferred over Qt5.
set(EASY_PROFILER_QT_VERSION "" CACHE STRING "Qt major version used by profiler_gui (5, 6 or empty for auto-detection)")
set_property(CACHE EASY_PROFILER_QT_VERSION PROPERTY STRINGS "" 5 6)

if (EASY_PROFILER_QT_VERSION)
    set(_easy_qt_candidates Qt${EASY_PROFILER_QT_VERSION})
else ()
    set(_easy_qt_candidates Qt6 Qt5)
endif ()

find_package(QT NAMES ${_easy_qt_candidates} COMPONENTS Widgets QUIET)
if (QT_FOUND)
    find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets QUIET)
endif ()

set(_easy_qt_ok FALSE)
if (QT_FOUND AND Qt${QT_VERSION_MAJOR}Widgets_FOUND)
    if (QT_VERSION_MAJOR EQUAL 5 AND Qt5Widgets_VERSION VERSION_LESS "5.12")
        message(WARNING "profiler_gui requires Qt >= 5.12 (found ${Qt5Widgets_VERSION}). GUI will not be built.")
    else ()
        set(_easy_qt_ok TRUE)
    endif ()
endif ()

if (_easy_qt_ok)
    message(STATUS "Using Qt v${Qt${QT_VERSION_MAJOR}Widgets_VERSION}")
    if (NOT("${CMAKE_BUILD_TYPE}" STREQUAL "Debug") AND WIN32)
        set(APPLICATION_PLATFORM WIN32)
    endif ()
    add_executable(profiler_gui ${APPLICATION_PLATFORM}
        main.cpp
        arbitrary_value_inspector.h
        arbitrary_value_inspector.cpp
        arbitrary_value_tooltip.h
        arbitrary_value_tooltip.cpp
        blocks_graphics_view.h
        blocks_graphics_view.cpp
        blocks_tree_widget.h
        blocks_tree_widget.cpp
        bookmarks_editor.h
        bookmarks_editor.cpp
        common_functions.h
        common_functions.cpp
        common_types.h
        descriptors_tree_widget.h
        descriptors_tree_widget.cpp
        dialog.h
        dialog.cpp
        file_reader.h
        file_reader.cpp
        fps_widget.h
        fps_widget.cpp
        globals.h
        globals.cpp
        globals_qobjects.cpp
        graphics_block_item.h
        graphics_block_item.cpp
        graphics_image_item.h
        graphics_image_item.cpp
        graphics_ruler_item.h
        graphics_ruler_item.cpp
        graphics_scrollbar.h
        graphics_scrollbar.cpp
        graphics_slider_area.h
        graphics_slider_area.cpp
        main_window.h
        main_window.cpp
        round_progress_widget.h
        round_progress_widget.cpp
        socket_listener.h
        socket_listener.cpp
        text_highlighter.h
        text_highlighter.cpp
        timer.h
        timer.cpp
        thread_pool.h
        thread_pool.cpp
        thread_pool_task.h
        thread_pool_task.cpp
        tree_widget_item.h
        tree_widget_item.cpp
        tree_widget_loader.h
        tree_widget_loader.cpp
        window_header.h
        window_header.cpp
        qt_compat.h
        resources.qrc
        resources.rc
    )
    target_link_libraries(profiler_gui Qt${QT_VERSION_MAJOR}::Widgets easy_profiler)
    # The GUI code uses C++17 (structured bindings). Qt6 imposes it implicitly, Qt5 does not.
    target_compile_features(profiler_gui PRIVATE cxx_std_17)
    if (WIN32)
        target_compile_definitions(profiler_gui PRIVATE -D_WIN32_WINNT=0x0600)
    endif ()
    if (MINGW)
        target_compile_definitions(profiler_gui PRIVATE -DSTRSAFE_NO_DEPRECATE)
    endif ()

    install(
        TARGETS
        profiler_gui
        RUNTIME
        DESTINATION
        bin
    )

    set_property(TARGET profiler_gui PROPERTY INSTALL_RPATH_USE_LINK_PATH TRUE)
else ()
    message(STATUS "INFO\n\n\tQt5 (>= 5.12) / Qt6 not found! Generating EasyProfiler projects without GUI.\n")
endif ()

