cmake_minimum_required(VERSION 3.0)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${LIBJSONC_CFLAGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
include_directories(SYSTEM ${LIBJSONC_INCLUDE_DIRS})

set(SRCS
    crypt
    piano
    request
    response
    list)

add_library(piano SHARED ${SRCS})
