set (CMAKE_C_FLAGS -Wall)

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
	${CMAKE_CURRENT_BINARY_DIR}/config.h)

include_directories (${CMAKE_CURRENT_SOURCE_DIR}/../../libwaitress/src
  ${CMAKE_CURRENT_BINARY_DIR} )

add_library (wardrobe STATIC wardrobe.c md5.c)
target_link_libraries (wardrobe waitress)
