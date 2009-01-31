find_package (CURL REQUIRED)

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
	${CMAKE_CURRENT_BINARY_DIR}/config.h)

include_directories (${CURL_INCLUDE_DIR})

add_library (wardrobe STATIC main.c md5.c)
