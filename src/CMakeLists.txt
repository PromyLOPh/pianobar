cmake_minimum_required(VERSION 3.0)

add_subdirectory(libpiano)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${LIBAV_CFLAGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${LIBCURL_CFLAGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${LIBGNUTLS_CFLAGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${LIBJSONC_CFLAGS}")

include_directories(SYSTEM ${LIBAV_INCLUDE_DIRS})
include_directories(SYSTEM ${LIBCURL_INCLUDE_DIRS})
include_directories(SYSTEM ${LIBGNUTLS_INCLUDE_DIRS})
include_directories(SYSTEM ${LIBJSONC_INCLUDE_DIRS})
include_directories(libpiano)

set(SRCS
    main
    player
    settings
    terminal
    ui
    ui_act
    ui_dispatch
    ui_readline)

add_executable(pianobar ${SRCS})
target_link_libraries(pianobar
    # Internal
    piano

    # External
    ao
    pthread
    m
    ${LIBAV_LIBRARIES}
    ${LIBGNUTLS_LIBRARIES}
    gcrypt
    ${LIBJSONC_LIBRARIES}
    ${LIBCURL_LIBRARIES})
