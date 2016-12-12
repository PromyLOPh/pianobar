# makefile of pianobar

PREFIX:=/usr/local
BINDIR:=${PREFIX}/bin
LIBDIR:=${PREFIX}/lib
INCDIR:=${PREFIX}/include
MANDIR:=${PREFIX}/share/man
DYNLINK:=0
CFLAGS?=-O2 -DNDEBUG

ifeq (${CC},cc)
	OS := $(shell uname)
	ifeq (${OS},Darwin)
		CC:=gcc -std=c99
	else ifeq (${OS},FreeBSD)
		CC:=cc -std=c99
	else ifeq (${OS},OpenBSD)
		CC:=cc -std=c99
	else
		CC:=c99
	endif
endif

PIANOBAR_DIR:=src
PIANOBAR_SRC:=\
		${PIANOBAR_DIR}/main.c \
		${PIANOBAR_DIR}/player.c \
		${PIANOBAR_DIR}/settings.c \
		${PIANOBAR_DIR}/terminal.c \
		${PIANOBAR_DIR}/ui_act.c \
		${PIANOBAR_DIR}/ui.c \
		${PIANOBAR_DIR}/ui_readline.c \
		${PIANOBAR_DIR}/ui_dispatch.c
PIANOBAR_OBJ:=${PIANOBAR_SRC:.c=.o}

LIBPIANO_DIR:=src/libpiano
LIBPIANO_SRC:=\
		${LIBPIANO_DIR}/crypt.c \
		${LIBPIANO_DIR}/piano.c \
		${LIBPIANO_DIR}/request.c \
		${LIBPIANO_DIR}/response.c \
		${LIBPIANO_DIR}/list.c
LIBPIANO_OBJ:=${LIBPIANO_SRC:.c=.o}
LIBPIANO_RELOBJ:=${LIBPIANO_SRC:.c=.lo}
LIBPIANO_INCLUDE:=${LIBPIANO_DIR}

LIBAV_CFLAGS:=$(shell pkg-config --cflags libavcodec libavformat libavutil libavfilter)
LIBAV_LDFLAGS:=$(shell pkg-config --libs libavcodec libavformat libavutil libavfilter)

LIBCURL_CFLAGS:=$(shell pkg-config --cflags libcurl)
LIBCURL_LDFLAGS:=$(shell pkg-config --libs libcurl)

LIBGCRYPT_CFLAGS:=
LIBGCRYPT_LDFLAGS:=-lgcrypt

LIBJSONC_CFLAGS:=$(shell pkg-config --cflags json-c 2>/dev/null || pkg-config --cflags json)
LIBJSONC_LDFLAGS:=$(shell pkg-config --libs json-c 2>/dev/null || pkg-config --libs json)

LIBAO_CFLAGS:=$(shell pkg-config --cflags ao)
LIBAO_LDFLAGS:=$(shell pkg-config --libs ao)

# combine all flags
ALL_CFLAGS:=${CFLAGS} -I ${LIBPIANO_INCLUDE} \
			${LIBAV_CFLAGS} ${LIBCURL_CFLAGS} \
			${LIBGCRYPT_CFLAGS} ${LIBJSONC_CFLAGS} \
			${LIBAO_CFLAGS}
ALL_LDFLAGS:=${LDFLAGS} -lpthread -lm \
			${LIBAV_LDFLAGS} ${LIBCURL_LDFLAGS} \
			${LIBGCRYPT_LDFLAGS} ${LIBJSONC_LDFLAGS} \
			${LIBAO_LDFLAGS}

# Be verbose if V=1 (gnu autotools’ --disable-silent-rules)
SILENTCMD:=@
SILENTECHO:=@echo
ifeq (${V},1)
	SILENTCMD:=
	SILENTECHO:=@true
endif

# build pianobar
ifeq (${DYNLINK},1)
pianobar: ${PIANOBAR_OBJ} libpiano.so.0
	${SILENTECHO} "  LINK  $@"
	${SILENTCMD}${CC} -o $@ ${PIANOBAR_OBJ} -L. -lpiano ${ALL_LDFLAGS}
else
pianobar: ${PIANOBAR_OBJ} ${LIBPIANO_OBJ}
	${SILENTECHO} "  LINK  $@"
	${SILENTCMD}${CC} -o $@ ${PIANOBAR_OBJ} ${LIBPIANO_OBJ} ${ALL_LDFLAGS}
endif

# build shared and static libpiano
libpiano.so.0: ${LIBPIANO_RELOBJ} ${LIBPIANO_OBJ}
	${SILENTECHO} "  LINK  $@"
	${SILENTCMD}${CC} -shared -Wl,-soname,libpiano.so.0 -o libpiano.so.0.0.0 \
			${LIBPIANO_RELOBJ} ${ALL_LDFLAGS}
	${SILENTCMD}ln -fs libpiano.so.0.0.0 libpiano.so.0
	${SILENTCMD}ln -fs libpiano.so.0 libpiano.so
	${SILENTECHO} "    AR  libpiano.a"
	${SILENTCMD}${AR} rcs libpiano.a ${LIBPIANO_OBJ}


-include $(PIANOBAR_SRC:.c=.d)
-include $(LIBPIANO_SRC:.c=.d)

# build standard object files
%.o: %.c
	${SILENTECHO} "    CC  $<"
	${SILENTCMD}${CC} -c -o $@ ${ALL_CFLAGS} -MMD -MF $*.d -MP $<

# create position independent code (for shared libraries)
%.lo: %.c
	${SILENTECHO} "    CC  $< (PIC)"
	${SILENTCMD}${CC} -c -fPIC -o $@ ${ALL_CFLAGS} -MMD -MF $*.d -MP $<

clean:
	${SILENTECHO} " CLEAN"
	${SILENTCMD}${RM} ${PIANOBAR_OBJ} ${LIBPIANO_OBJ} \
			${LIBPIANO_RELOBJ} pianobar libpiano.so* \
			libpiano.a $(PIANOBAR_SRC:.c=.d) $(LIBPIANO_SRC:.c=.d)

all: pianobar

ifeq (${DYNLINK},1)
install: pianobar install-libpiano
else
install: pianobar
endif
	install -d ${DESTDIR}${BINDIR}/
	install -m755 pianobar ${DESTDIR}${BINDIR}/
	install -d ${DESTDIR}${MANDIR}/man1/
	install -m644 contrib/pianobar.1 ${DESTDIR}${MANDIR}/man1/

install-libpiano:
	install -d ${DESTDIR}${LIBDIR}/
	install -m644 libpiano.so.0.0.0 ${DESTDIR}${LIBDIR}/
	ln -fs libpiano.so.0.0.0 ${DESTDIR}${LIBDIR}/libpiano.so.0
	ln -fs libpiano.so.0 ${DESTDIR}${LIBDIR}/libpiano.so
	install -m644 libpiano.a ${DESTDIR}${LIBDIR}/
	install -d ${DESTDIR}${INCDIR}/
	install -m644 src/libpiano/piano.h ${DESTDIR}${INCDIR}/

uninstall:
	$(RM) ${DESTDIR}/${BINDIR}/pianobar \
	${DESTDIR}/${MANDIR}/man1/pianobar.1 \
	${DESTDIR}/${LIBDIR}/libpiano.so.0.0.0 \
	${DESTDIR}/${LIBDIR}/libpiano.so.0 \
	${DESTDIR}/${LIBDIR}/libpiano.so \
	${DESTDIR}/${LIBDIR}/libpiano.a \
	${DESTDIR}/${INCDIR}/piano.h

.PHONY: install install-libpiano uninstall test debug all
