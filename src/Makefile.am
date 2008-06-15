bin_PROGRAMS = pianobar

pianobar_SOURCES = main.c terminal.c terminal.h settings.c settings.h
pianobar_CPPFLAGS = ${LIBCURL_CFLAGS} ${LIBAO_CFLAGS} ${LIBXML_CFLAGS} -I../libpiano
pianobar_LDADD = ${LIBCURL_LIBS} ${LIBAO_LIBS} ${LIBFAAD_LIBS} ${LIBREADLINE_LIBS} ${LIBXML_LIBS} -lpthread ../libpiano/libpiano.la
dist_man1_MANS = pianobar.1
