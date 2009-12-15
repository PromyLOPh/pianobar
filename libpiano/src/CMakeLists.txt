set (CMAKE_C_FLAGS -Wall)

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
	${CMAKE_CURRENT_BINARY_DIR}/config.h)

include_directories (${CMAKE_CURRENT_SOURCE_DIR}/../../libwaitress/src
        ${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../../libezxml/src)

add_library (piano STATIC crypt.c http.c piano.c xml.c)
target_link_libraries (piano waitress ezxml)

