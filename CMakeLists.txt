cmake_minimum_required (VERSION 2.4)

set (PACKAGE pianobar)
project (${PACKAGE} C)

add_subdirectory (libpiano)
add_subdirectory (libwardrobe)
add_subdirectory (libwaitress)
add_subdirectory (src)
