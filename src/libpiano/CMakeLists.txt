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

# Shared version
add_library(piano SHARED ${SRCS})
set_target_properties(piano PROPERTIES SOVERSION 0.0.0)

# Static version
add_library(piano-static STATIC ${SRCS})
set_target_properties(piano-static PROPERTIES OUTPUT_NAME piano)

if (DYNLINK)
    set(LIBPIANO_COMPONENT "COMPONENT libpiano")
else ()
    set(LIBPIANO_COMPONENT)
endif ()
install(TARGETS piano piano-static
    DESTINATION lib
    ${LIBPIANO_COMPONENT})
install(FILES
    piano.h
    DESTINATION include
    PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
    ${LIBPIANO_COMPONENT})
