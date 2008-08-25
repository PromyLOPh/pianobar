bin_PROGRAMS = pianobar

pianobar_SOURCES = main.c main.h terminal.c terminal.h settings.c settings.h \
		player.c player.h ui.c ui.h ui_act.c ui_act.h
pianobar_CPPFLAGS = ${LIBCURL_CFLAGS} ${LIBAO_CFLAGS} ${LIBXML_CFLAGS} \
		-I../libpiano/src -I../libwardrobe/src
pianobar_LDADD = ${LIBCURL_LIBS} ${LIBAO_LIBS} ${LIBFAAD_LIBS} \
		${LIBREADLINE_LIBS} ${LIBXML_LIBS} ${LIBPTHREAD_LIBS} ${LIBM_LIBS} \
		../libpiano/src/libpiano.la ../libwardrobe/src/libwardrobe.la
dist_man1_MANS = pianobar.1
