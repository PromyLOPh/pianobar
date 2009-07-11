set (CMAKE_C_FLAGS -Wall)

add_library (ezxml STATIC ezxml.c)
target_link_libraries (ezxml)
