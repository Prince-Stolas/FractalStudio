cmake_minimum_required(VERSION 3.21)
project(FractalPlayground LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MINGW AND NOT DEFINED Qt6_DIR)
    list(PREPEND CMAKE_PREFIX_PATH "C:/msys64/mingw64/qt6-static")
endif()
find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets OpenGLWidgets)
get_target_property(QT_CORE_TYPE Qt6::Core TYPE)
if(NOT QT_CORE_TYPE STREQUAL "STATIC_LIBRARY")
    message(FATAL_ERROR "A static Qt installation is required. Set CMAKE_PREFIX_PATH to its installation prefix.")
endif()
if(MINGW)
    # MSYS2's static Qt package discovers shared third-party imports by default.
    get_property(imported_targets DIRECTORY PROPERTY IMPORTED_TARGETS)
    foreach(target IN LISTS imported_targets)
        foreach(property IMPORTED_IMPLIB IMPORTED_IMPLIB_RELEASE IMPORTED_IMPLIB_DEBUG IMPORTED_LOCATION IMPORTED_LOCATION_RELEASE INTERFACE_LINK_LIBRARIES)
            get_target_property(value "${target}" "${property}")
            if(value)
                string(REPLACE ".dll.a" ".a" static_value "${value}")
                if(NOT value STREQUAL static_value)
                    set_target_properties("${target}" PROPERTIES "${property}" "${static_value}")
                endif()
            endif()
        endforeach()
    endforeach()
endif()
qt_add_executable(fractal WIN32
    playground.cpp
    equation.cpp
    fractal_view.cpp
    numeric_slider.cpp
    gradient_editor.cpp
    workspace.cpp
    mainwindow.cpp
    mainwindow_ui.cpp
    mainwindow_tools.cpp
    regression.cpp
)
qt_add_resources(ui_icon_sources ui_resources.qrc)
target_sources(fractal PRIVATE ${ui_icon_sources})
target_link_libraries(fractal PRIVATE Qt6::Widgets Qt6::OpenGLWidgets)
qt_import_plugins(fractal EXCLUDE_BY_TYPE imageformats)
if(MINGW)
    set_property(TARGET harfbuzz::harfbuzz APPEND PROPERTY INTERFACE_LINK_LIBRARIES graphite2 rpcrt4)
    set_property(TARGET Freetype::Freetype APPEND PROPERTY INTERFACE_LINK_LIBRARIES brotlidec brotlicommon bz2)
endif()
if(MINGW)
    target_compile_options(fractal PRIVATE -Wall -Wextra)
    target_link_options(fractal PRIVATE -static -static-libgcc -static-libstdc++)
endif()
