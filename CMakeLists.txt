cmake_minimum_required(VERSION 3.15)
project(QService)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_CXX_FLAGS_RELEASE "-Ox")
set(CMAKE_CXX_FLAGS_DEBUG "-Od -DDEBUG")

set(CMAKE_PREFIX_PATH ${QT5_DIR})

find_package(Qt5Core REQUIRED)

add_library(QService SHARED
        QService/QService.cpp QService/QService.h)

target_include_directories(QService PUBLIC .
        ${Qt5Core_INCLUDES})

target_link_libraries(QService
        ${Qt5Core_LIBRARIES})

target_compile_definitions(QService PRIVATE -DSHARED_LIBRARY)

target_compile_options(QService PUBLIC "/arch:AVX2" "/nologo" "/GS" "/W3" "/WX" "/Gm-" "/sdl" "/Oi"
        "/Zc:wchar_t" "/Zc:inline" "/Zc:forScope"
        "${Qt5Widgets_EXECUTABLE_COMPILE_FLAGS}")
