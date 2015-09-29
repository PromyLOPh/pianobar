cmake_minimum_required(VERSION 3.0)

project(pianobar)

find_package(PkgConfig REQUIRED)

pkg_check_modules(LIBAV REQUIRED libavcodec libavformat libavutil libavfilter)
pkg_check_modules(LIBCURL REQUIRED libcurl)
pkg_check_modules(LIBGNUTLS REQUIRED gnutls)
pkg_search_module(LIBJSONC REQUIRED json-c json)

add_subdirectory(src)
