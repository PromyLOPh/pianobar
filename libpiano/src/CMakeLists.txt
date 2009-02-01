set (CMAKE_C_FLAGS -Wall)

find_package (LibXml2 REQUIRED)
find_package (CURL REQUIRED)

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
	${CMAKE_CURRENT_BINARY_DIR}/config.h)

include_directories (${LIBXML2_INCLUDE_DIR} ${CURL_INCLUDE_DIRS})
add_library (piano STATIC crypt.c http.c main.c xml.c)
target_link_libraries (${CURL_LIBRARIES} ${LIBXML_LIBRARIES})
