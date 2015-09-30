cmake_minimum_required(VERSION 3.0)

# Set compiler prior to invoking `project` function or else it won't take affect
if ("Darwin" STREQUAL "${CMAKE_SYSTEM_NAME}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
    set(CMAKE_C_COMPILER "gcc")
elseif ("FreeBSD" STREQUAL "${CMAKE_SYSTEM_NAME}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
    set(CMAKE_C_COMPILER "cc")
elseif ("OpenBSD" STREQUAL "${CMAKE_SYSTEM_NAME}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
    set(CMAKE_C_COMPILER "cc")
else ()
    set(CMAKE_C_COMPILER "c99")
endif ()

project(pianobar
    VERSION 2014.09.28)

option(DYNLINK "Link libpiano dynamically" OFF)

find_package(PkgConfig REQUIRED)

pkg_check_modules(LIBAV REQUIRED libavcodec libavformat libavutil libavfilter)
pkg_check_modules(LIBCURL REQUIRED libcurl)
pkg_check_modules(LIBGNUTLS REQUIRED gnutls)
pkg_search_module(LIBJSONC REQUIRED json-c json)

add_subdirectory(src)

install(FILES
    contrib/pianobar.1
    DESTINATION share/man/man1
    PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

include(CPackConfig.cmake)
