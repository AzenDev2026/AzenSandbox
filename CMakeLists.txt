cmake_minimum_required(VERSION 3.16)
project(azen-sandbox VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(X11 REQUIRED)

add_executable(azen-sandbox
    src/main.cpp
    src/theme.cpp
    src/apps.cpp
    src/x11.cpp
    src/proc.cpp
    src/boot.cpp
    src/embedded.cpp
    src/desktop.cpp
    src/taskbar.cpp
    src/startmenu.cpp
    src/sandbox.cpp
    resources.qrc
)

target_include_directories(azen-sandbox PRIVATE src)
# Private QtGui headers (QPlatformNativeInterface for the xcb connection).
if(Qt6Gui_VERSION)
    target_include_directories(azen-sandbox PRIVATE
        "/usr/include/qt6/QtGui/${Qt6Gui_VERSION}")
endif()
target_link_libraries(azen-sandbox PRIVATE Qt6::Widgets X11::X11 xcb xcb-shape)

install(TARGETS azen-sandbox RUNTIME DESTINATION bin)
