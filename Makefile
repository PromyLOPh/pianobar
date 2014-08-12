# makefile of pianobar

ifeq (${PREFIX},)
	PREFIX:=/usr/local
endif
BINDIR:=${PREFIX}/bin
LIBDIR:=${PREFIX}/lib
INCDIR:=${PREFIX}/include
MANDIR:=${PREFIX}/share/man
DYNLINK:=0

# Respect environment variables set by user; does not work with :=
ifeq (${CFLAGS},)
	CFLAGS=-O2 -DNDEBUG
endif
ifeq (${CC},cc)
	OS := $(shell uname)
	ifeq (${OS},Darwin)
		CC=gcc -std=c99
	else ifeq (${OS},FreeBSD)
		CC=cc -std=c99
	else ifeq (${OS},OpenBSD)
		CC=cc -std=c99
	else
		CC=c99
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
PIANOBAR_HDR:=\
		${PIANOBAR_DIR}/player.h \
		${PIANOBAR_DIR}/settings.h \
		${PIANOBAR_DIR}/terminal.h \
		${PIANOBAR_DIR}/ui_act.h \
		${PIANOBAR_DIR}/ui.h \
		${PIANOBAR_DIR}/ui_readline.h \
		${PIANOBAR_DIR}/main.h \
		${PIANOBAR_DIR}/config.h
PIANOBAR_OBJ:=${PIANOBAR_SRC:.c=.o}

LIBPIANO_DIR:=src/libpiano
LIBPIANO_SRC:=\
		${LIBPIANO_DIR}/crypt.c \
		${LIBPIANO_DIR}/piano.c \
		${LIBPIANO_DIR}/request.c \
		${LIBPIANO_DIR}/response.c \
		${LIBPIANO_DIR}/list.c
LIBPIANO_HDR:=\
		${LIBPIANO_DIR}/config.h \
		${LIBPIANO_DIR}/crypt.h \
		${LIBPIANO_DIR}/piano.h \
		${LIBPIANO_DIR}/piano_private.h
LIBPIANO_OBJ:=${LIBPIANO_SRC:.c=.o}
LIBPIANO_RELOBJ:=${LIBPIANO_SRC:.c=.lo}
LIBPIANO_INCLUDE:=${LIBPIANO_DIR}

LIBWAITRESS_DIR:=src/libwaitress
LIBWAITRESS_SRC:=${LIBWAITRESS_DIR}/waitress.c
LIBWAITRESS_HDR:=\
		${LIBWAITRESS_DIR}/config.h \
		${LIBWAITRESS_DIR}/waitress.h
LIBWAITRESS_OBJ:=${LIBWAITRESS_SRC:.c=.o}
LIBWAITRESS_RELOBJ:=${LIBWAITRESS_SRC:.c=.lo}
LIBWAITRESS_INCLUDE:=${LIBWAITRESS_DIR}

LIBWAITRESS_TEST_SRC=${LIBWAITRESS_DIR}/waitress-test.c
LIBWAITRESS_TEST_OBJ:=${LIBWAITRESS_TEST_SRC:.c=.o}

LIBAV_CFLAGS=$(shell pkg-config --cflags libavcodec libavformat libavutil libavfilter)
LIBAV_LDFLAGS=$(shell pkg-config --libs libavcodec libavformat libavutil libavfilter)

LIBGNUTLS_CFLAGS:=$(shell pkg-config --cflags gnutls)
LIBGNUTLS_LDFLAGS:=$(shell pkg-config --libs gnutls)

LIBGCRYPT_CFLAGS:=
LIBGCRYPT_LDFLAGS:=-lgcrypt

LIBJSONC_CFLAGS:=$(shell pkg-config --cflags json-c 2>/dev/null || pkg-config --cflags json)
LIBJSONC_LDFLAGS:=$(shell pkg-config --libs json-c 2>/dev/null || pkg-config --libs json)

# combine all flags
ALL_CFLAGS:=${CFLAGS} -I ${LIBPIANO_INCLUDE} -I ${LIBWAITRESS_INCLUDE} \
			${LIBAV_CFLAGS} ${LIBGNUTLS_CFLAGS} \
			${LIBGCRYPT_CFLAGS} ${LIBJSONC_CFLAGS}
ALL_LDFLAGS:=${LDFLAGS} -lao -lpthread -lm \
			${LIBAV_LDFLAGS} ${LIBGNUTLS_LDFLAGS} \
			${LIBGCRYPT_LDFLAGS} ${LIBJSONC_LDFLAGS}

# build pianobar
ifeq (${DYNLINK},1)
pianobar: ${PIANOBAR_OBJ} ${PIANOBAR_HDR} libpiano.so.0
	@echo "  LINK  $@"
	@${CC} -o $@ ${PIANOBAR_OBJ} -L. -lpiano ${ALL_LDFLAGS}
else
pianobar: ${PIANOBAR_OBJ} ${PIANOBAR_HDR} ${LIBPIANO_OBJ} ${LIBWAITRESS_OBJ} \
		${LIBWAITRESS_HDR}
	@echo "  LINK  $@"
	@${CC} -o $@ ${PIANOBAR_OBJ} ${LIBPIANO_OBJ} ${LIBWAITRESS_OBJ} \
			${ALL_LDFLAGS}
endif

# build shared and static libpiano
libpiano.so.0: ${LIBPIANO_RELOBJ} ${LIBPIANO_HDR} ${LIBWAITRESS_RELOBJ} \
		${LIBWAITRESS_HDR} ${LIBPIANO_OBJ} ${LIBWAITRESS_OBJ}
	@echo "  LINK  $@"
	@${CC} -shared -Wl,-soname,libpiano.so.0 -o libpiano.so.0.0.0 \
			${LIBPIANO_RELOBJ} ${LIBWAITRESS_RELOBJ} ${ALL_LDFLAGS}
	@ln -s libpiano.so.0.0.0 libpiano.so.0
	@ln -s libpiano.so.0 libpiano.so
	@echo "    AR  libpiano.a"
	@${AR} rcs libpiano.a ${LIBPIANO_OBJ} ${LIBWAITRESS_OBJ}


-include $(PIANOBAR_SRC:.c=.d)
-include $(LIBPIANO_SRC:.c=.d)
-include $(LIBWAITRESS_SRC:.c=.d)

# build standard object files
%.o: %.c
	@echo "    CC  $<"
	@${CC} -c -o $@ ${ALL_CFLAGS} -MMD -MF $*.d -MP $<

# create position independent code (for shared libraries)
%.lo: %.c
	@echo "    CC  $< (PIC)"
	@${CC} -c -fPIC -o $@ ${ALL_CFLAGS} -MMD -MF $*.d -MP $<

clean:
	@echo " CLEAN"
	@${RM} ${PIANOBAR_OBJ} ${LIBPIANO_OBJ} ${LIBWAITRESS_OBJ} ${LIBWAITRESS_OBJ}/test.o \
			${LIBPIANO_RELOBJ} ${LIBWAITRESS_RELOBJ} pianobar libpiano.so* \
			libpiano.a waitress-test $(PIANOBAR_SRC:.c=.d) $(LIBPIANO_SRC:.c=.d) \
			$(LIBWAITRESS_SRC:.c=.d)

all: pianobar

waitress-test: ${LIBWAITRESS_TEST_OBJ}
	${CC} ${LDFLAGS} ${LIBWAITRESS_TEST_OBJ} ${LIBGNUTLS_LDFLAGS} -o waitress-test

test: waitress-test
	./waitress-test

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
	ln -s libpiano.so.0.0.0 ${DESTDIR}${LIBDIR}/libpiano.so.0
	ln -s libpiano.so.0 ${DESTDIR}${LIBDIR}/libpiano.so
	install -m644 libpiano.a ${DESTDIR}${LIBDIR}/
	install -d ${DESTDIR}${INCDIR}/
	install -m644 src/libpiano/piano.h ${DESTDIR}${INCDIR}/

.PHONY: install install-libpiano test debug all
