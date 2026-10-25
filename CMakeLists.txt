cmake_minimum_required(VERSION 3.21)
project(FractalPlayground LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets OpenGLWidgets)
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
    readme_capture.cpp
)
qt_add_resources(ui_icon_sources ui_resources.qrc)
target_sources(fractal PRIVATE ${ui_icon_sources})
target_link_libraries(fractal PRIVATE Qt6::Widgets Qt6::OpenGLWidgets)
if(MINGW)
    target_compile_options(fractal PRIVATE -Wall -Wextra)
endif()
