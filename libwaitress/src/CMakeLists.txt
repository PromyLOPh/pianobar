set (CMAKE_C_FLAGS -Wall)

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
	${CMAKE_CURRENT_BINARY_DIR}/config.h)

add_library (waitress STATIC main.c)

