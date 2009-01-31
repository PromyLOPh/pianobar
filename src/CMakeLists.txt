set (CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})
find_package (LibXml2 REQUIRED)
find_package (CURL REQUIRED)
find_package (Faad REQUIRED)
find_package (Readline REQUIRED)
find_package (LibAo REQUIRED)
find_package (Threads REQUIRED)
find_library (LIBM m)

if (NOT CMAKE_USE_PTHREADS_INIT)
	message (FATAL_ERROR "pthread is currently required")
endif (NOT CMAKE_USE_PTHREADS_INIT)

if (NOT LIBM)
	message (FATAL_ERROR "libm is required")
endif (NOT LIBM)

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
	${CMAKE_CURRENT_BINARY_DIR}/config.h)

include_directories (
	${pianobar_SOURCE_DIR}/libpiano/src
	${pianobar_SOURCE_DIR}/libwardrobe/src
	${LIBXML2_INCLUDE_DIR}
	${CURL_INCLUDE_DIRS} ${READLINE_INCLUDE_DIR} ${FAAD_INCLUDE_DIRS}
	${LIBAO_INCLUDE_DIRS})

add_executable (pianobar main.c terminal.c settings.c player.c ui.c ui_act.c)
target_link_libraries (pianobar piano wardrobe ${CURL_LIBRARIES}
	${LIBXML2_LIBRARIES} ${READLINE_LIBRARY} ${FAAD_LIBRARY} ${LIBAO_LIBRARY}
	${CMAKE_THREAD_LIBS_INIT} ${LIBM})

install (TARGETS pianobar RUNTIME DESTINATION bin)
install (FILES pianobar.1 DESTINATION share/man/man1)
