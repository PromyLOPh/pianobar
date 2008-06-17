lib_LTLIBRARIES = libpiano.la

libpiano_la_SOURCES = crypt.c http.c main.c xml.c const.h crypt.h crypt_key_input.h crypt_key_output.h http.h main.h xml.h
include_HEADERS = piano.h
libpiano_la_CPPFLAGS = ${LIBCURL_CFLAGS} ${LIBXML_CFLAGS}
libpiano_la_LIBADD = ${LIBCURL_LIBS} ${LIBXML_LIBS}
