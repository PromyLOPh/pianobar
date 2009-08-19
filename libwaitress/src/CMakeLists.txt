INCLUDE (CheckFunctionExists)
INCLUDE (CheckLibraryExists)

set (CMAKE_C_FLAGS -Wall)
set (EXTRA_LIBS "")

# required on sunos
CHECK_FUNCTION_EXISTS("connect" CMAKE_HAVE_CONNECT)
	IF(NOT CMAKE_HAVE_CONNECT)
		CHECK_LIBRARY_EXISTS("socket" "connect" "" CMAKE_LIB_SOCKET_HAS_CONNECT)
		IF (CMAKE_LIB_SOCKET_HAS_CONNECT)
			SET (EXTRA_LIBS -lsocket)
		ENDIF (CMAKE_LIB_SOCKET_HAS_CONNECT)
ENDIF(NOT CMAKE_HAVE_CONNECT)

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
	${CMAKE_CURRENT_BINARY_DIR}/config.h)

include_directories( ${CMAKE_CURRENT_BINARY_DIR} )

add_library (waitress STATIC main.c)
target_link_libraries (waitress ${EXTRA_LIBS})

