cmake_minimum_required(VERSION 3.16)
project(DesktopAIMessenger VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# 使用 Qt6 (如果系统装的是 Qt5 就改成 Qt5)
find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

add_executable(DesktopAIMessenger WIN32
    main.cpp
    avatarutils.cpp
    claudeclient.cpp
    deepseekclient.cpp
    markdownutils.cpp
    skillmanager.cpp
    mainwindow.h
    mainwindow.cpp
    mainwindow_network.cpp
)

target_link_libraries(DesktopAIMessenger PRIVATE Qt6::Widgets Qt6::Network)
