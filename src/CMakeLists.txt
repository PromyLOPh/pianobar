set (CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})
set (CMAKE_C_FLAGS -Wall)
set (ENABLE_MAD 0)
set (ENABLE_FAAD 0)

# This removes a warning on newer versions of cmake
if(COMMAND cmake_policy)
  cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

option (USE_FAAD "Use libfaad for aac decoding (if available)" on)
option (USE_MAD "Use libmad for mp3 decoding (if available)" on)

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

if (FAAD_FOUND AND USE_FAAD)
	message (STATUS "Found libfaad, enabling aac decoding")
	set (ENABLE_FAAD 1)
elseif (FAAD_FOUND AND NOT USE_FAAD)
	message (STATUS "Found libfaad, but disabling aac decoding by request")
elseif (NOT FAAD_FOUND AND USE_FAAD)
	message (STATUS "libfaad not found but requested")
endif (FAAD_FOUND AND USE_FAAD)

if (MAD_FOUND AND USE_MAD)
	message (STATUS "Found libmad, enabling mp3 decoding")
	set (ENABLE_MAD 1)
elseif (MAD_FOUND AND NOT USE_MAD)
	message (STATUS "Found libmad, but disabling mp3 decoding by request")
elseif (NOT MAD_FOUND AND USE_MAD)
	message (STATUS "libmad not found but requested")
endif (MAD_FOUND AND USE_MAD)

# check whether faad and/or mad are available and enabled
if (NOT ENABLE_FAAD AND NOT ENABLE_MAD)
	message (FATAL_ERROR "libmad and/or libfaad are required.")
endif (NOT ENABLE_FAAD AND NOT ENABLE_MAD)

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
	${CMAKE_CURRENT_BINARY_DIR}/config.h)

include_directories (
	${pianobar_SOURCE_DIR}/libpiano/src
	${pianobar_SOURCE_DIR}/libwardrobe/src
	${pianobar_SOURCE_DIR}/libwaitress/src
	${CMAKE_CURRENT_BINARY_DIR}
	${FAAD_INCLUDE_DIRS} ${LIBAO_INCLUDE_DIRS}
	${MAD_INCLUDE_DIRS})

add_executable (pianobar main.c terminal.c settings.c player.c ui.c ui_act.c
	ui_readline.c)
target_link_libraries (pianobar piano wardrobe waitress ${FAAD_LIBRARIES}
	${LIBAO_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${MAD_LIBRARIES} ${LIBM})

install (TARGETS pianobar RUNTIME DESTINATION bin)
install (FILES pianobar.1 DESTINATION share/man/man1)
