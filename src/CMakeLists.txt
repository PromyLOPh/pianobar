set (CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})
set (CMAKE_C_FLAGS -Wall)

find_package (LibXml2 REQUIRED)
find_package (CURL REQUIRED)
find_package (Readline REQUIRED)
find_package (LibAo REQUIRED)

# find threading implementation
find_package (Threads REQUIRED)
if (NOT CMAKE_USE_PTHREADS_INIT)
	message (FATAL_ERROR "pthread is currently required")
endif (NOT CMAKE_USE_PTHREADS_INIT)

# check for libm
find_library (LIBM m)
if (NOT LIBM)
	message (FATAL_ERROR "libm is required")
endif (NOT LIBM)

# check for audio decoding library
find_package (Faad)
find_package (Mad)
if (NOT FAAD_FOUND AND NOT MAD_FOUND)
	message (FATAL_ERROR "libmad and/or libfaad are required.")
endif (NOT FAAD_FOUND AND NOT MAD_FOUND)
if (FAAD_FOUND)
	message (STATUS "Found libfaad, enabling aac decoding")
	set (ENABLE_FAAD 1)
endif (FAAD_FOUND)
if (MAD_FOUND)
	message (STATUS "Found libmad, enabling mp3 decoding")
	set (ENABLE_MAD 1)
endif (MAD_FOUND)

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
	${CMAKE_CURRENT_BINARY_DIR}/config.h)

include_directories (
	${pianobar_SOURCE_DIR}/libpiano/src
	${pianobar_SOURCE_DIR}/libwardrobe/src
	${LIBXML2_INCLUDE_DIR}
	${CURL_INCLUDE_DIRS} ${READLINE_INCLUDE_DIR} ${FAAD_INCLUDE_DIRS}
	${LIBAO_INCLUDE_DIRS} ${MAD_INCLUDE_DIRS})

add_executable (pianobar main.c terminal.c settings.c player.c ui.c ui_act.c)
target_link_libraries (pianobar piano wardrobe ${CURL_LIBRARIES}
	${LIBXML2_LIBRARIES} ${READLINE_LIBRARY} ${FAAD_LIBRARY} ${LIBAO_LIBRARY}
	${CMAKE_THREAD_LIBS_INIT} ${MAD_LIBRARIES} ${LIBM})

install (TARGETS pianobar RUNTIME DESTINATION bin)
install (FILES pianobar.1 DESTINATION share/man/man1)
