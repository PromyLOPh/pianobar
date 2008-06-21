bin_PROGRAMS = pianobar

pianobar_SOURCES = main.c terminal.c terminal.h settings.c settings.h \
		player.c player.h
pianobar_CPPFLAGS = ${LIBCURL_CFLAGS} ${LIBAO_CFLAGS} ${LIBXML_CFLAGS} \
		-I../libpiano/src
pianobar_LDADD = ${LIBCURL_LIBS} ${LIBAO_LIBS} ${LIBFAAD_LIBS} \
		${LIBREADLINE_LIBS} ${LIBXML_LIBS} -lpthread \
		../libpiano/src/libpiano.la
dist_man1_MANS = pianobar.1
