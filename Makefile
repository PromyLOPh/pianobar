# makefile of pianobar

PREFIX:=/usr/local
BINDIR:=${PREFIX}/bin
LIBDIR:=${PREFIX}/lib
MANDIR:=${PREFIX}/share/man
CFLAGS:=-Wall -g -std=c99 -pedantic

PIANOBAR_DIR=src
PIANOBAR_SRC=\
		${PIANOBAR_DIR}/main.c \
		${PIANOBAR_DIR}/player.c \
		${PIANOBAR_DIR}/settings.c \
		${PIANOBAR_DIR}/terminal.c \
		${PIANOBAR_DIR}/ui_act.c \
		${PIANOBAR_DIR}/ui.c \
		${PIANOBAR_DIR}/ui_readline.c
PIANOBAR_HDR=\
		${PIANOBAR_DIR}/player.h \
		${PIANOBAR_DIR}/settings.h \
		${PIANOBAR_DIR}/terminal.h \
		${PIANOBAR_DIR}/ui_act.h \
		${PIANOBAR_DIR}/ui.h \
		${PIANOBAR_DIR}/ui_readline.h \
		${PIANOBAR_DIR}/main.h \
		${PIANOBAR_DIR}/config.h
PIANOBAR_OBJ=${PIANOBAR_SRC:.c=.o}

LIBPIANO_DIR=libpiano/src
LIBPIANO_SRC=\
		${LIBPIANO_DIR}/crypt.c \
		${LIBPIANO_DIR}/piano.c \
		${LIBPIANO_DIR}/xml.c
LIBPIANO_HDR=\
		${LIBPIANO_DIR}/config.h \
		${LIBPIANO_DIR}/crypt_key_output.h \
		${LIBPIANO_DIR}/xml.h \
		${LIBPIANO_DIR}/crypt.h \
		${LIBPIANO_DIR}/piano.h \
		${LIBPIANO_DIR}/crypt_key_input.h \
		${LIBPIANO_DIR}/piano_private.h
LIBPIANO_OBJ=${LIBPIANO_SRC:.c=.o}
LIBPIANO_RELOBJ=${LIBPIANO_SRC:.c=.lo}
LIBPIANO_INCLUDE=${LIBPIANO_DIR}

LIBWAITRESS_DIR=libwaitress/src
LIBWAITRESS_SRC=${LIBWAITRESS_DIR}/waitress.c
LIBWAITRESS_HDR=\
		${LIBWAITRESS_DIR}/config.h \
		${LIBWAITRESS_DIR}/waitress.h
LIBWAITRESS_OBJ=${LIBWAITRESS_SRC:.c=.o}
LIBWAITRESS_RELOBJ=${LIBWAITRESS_SRC:.c=.lo}
LIBWAITRESS_INCLUDE=${LIBWAITRESS_DIR}

LIBEZXML_SRC=libezxml/src/ezxml.c
LIBEZXML_HDR=libezxml/src/ezxml.h
LIBEZXML_OBJ=${LIBEZXML_SRC:.c=.o}
LIBEZXML_RELOBJ=${LIBEZXML_SRC:.c=.lo}
LIBEZXML_INCLUDE=libezxml/src

LIBAO_INCLUDE=/usr/include
LIBAO_LIB=-lao

LIBM_LIB=-lm

ifeq (${DISABLE_FAAD}, 1)
	LIBFAAD_INCLUDE=
	LIBFAAD_LIB=
	LIBFAAD_SWITCH=
else
	LIBFAAD_INCLUDE:=/usr/include
	LIBFAAD_LIB:=-lfaad
	LIBFAAD_SWITCH=-DENABLE_FAAD
endif

ifeq (${DISABLE_MAD}, 1)
	LIBMAD_INCLUDE=
	LIBMAD_LIB=
	LIBMAD_SWITCH=
else
	LIBMAD_INCLUDE:=/usr/include
	LIBMAD_LIB:=-lmad
	LIBMAD_SWITCH=-DENABLE_MAD
endif

PTHREAD_LIB=-pthread

# build pianobar
pianobar: ${PIANOBAR_OBJ} ${PIANOBAR_HDR} ${LIBPIANO_OBJ} ${LIBWAITRESS_OBJ} \
		${LIBWAITRESS_HDR} ${LIBEZXML_OBJ} ${LIBEZXML_HDR}
	${CC} ${CFLAGS} ${PIANOBAR_OBJ} ${LIBPIANO_OBJ} ${LIBWAITRESS_OBJ} \
			${LIBEZXML_OBJ} ${LIBAO_LIB} ${LIBFAAD_LIB} ${LIBMAD_LIB} \
			${PTHREAD_LIB} ${LIBM_LIB} -o $@

# build shared libpiano
libpiano: ${LIBPIANO_RELOBJ} ${LIBPIANO_HDR} ${LIBWAITRESS_RELOBJ} \
		${LIBWAITRESS_HDR} ${LIBEZXML_RELOBJ} ${LIBEZXML_HDR}
	${CC} -shared ${CFLAGS} ${LIBPIANO_RELOBJ} ${LIBWAITRESS_RELOBJ} \
			${LIBEZXML_RELOBJ} -o $@.so.0.0.0

%.o: %.c
	${CC} ${CFLAGS} -I ${LIBPIANO_INCLUDE} -I ${LIBWAITRESS_INCLUDE} \
			-I ${LIBEZXML_INCLUDE} ${LIBFAAD_SWITCH} ${LIBMAD_SWITCH} -c \
			-I ${LIBAO_INCLUDE} -I ${LIBFAAD_INCLUDE} -I ${LIBMAD_INCLUDE} \
			-o $@ $<

# create position independent code (for shared libraries)
%.lo: %.c
	${CC} ${CFLAGS} -I ${LIBPIANO_INCLUDE} -I ${LIBWAITRESS_INCLUDE} \
			-I ${LIBEZXML_INCLUDE} -c -fPIC -o $@ $<

clean:
	${RM} ${PIANOBAR_OBJ} ${LIBPIANO_OBJ} ${LIBWAITRESS_OBJ} ${LIBEZXML_OBJ} \
			${LIBPIANO_RELOBJ} ${LIBWAITRESS_RELOBJ} ${LIBEZXML_RELOBJ} pianobar \
			libpiano.so.0.0.0

all: pianobar libpiano

install: pianobar
	install -d ${DESTDIR}/${BINDIR}/
	install -m755 pianobar ${DESTDIR}/${BINDIR}/
	install -d ${DESTDIR}/${MANDIR}/man1/
	install -m644 src/pianobar.1 ${DESTDIR}/${MANDIR}/man1/

install-libpiano: libpiano
	install -d ${DESTDIR}/${LIBDIR}/
	install -m755 libpiano.so.0.0.0 ${DESTDIR}/${LIBDIR}/

