PROG = wmlaunchbutton
MAN  = ${PROG:=.1}
OBJS = ${PROG:=.o}
SRCS = ${OBJS:.o=.c}

PREFIX ?= /usr/local
MANPREFIX ?= ${PREFIX}/share/man
LOCALINC = /usr/local/include
LOCALLIB = /usr/local/lib
X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

DEFS = -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE -D_BSD_SOURCE
INCS = -I${LOCALINC} -I${X11INC}
LIBS = -L${LOCALLIB} -L${X11LIB} -lX11 -lXext -lXpm

bindir = ${DESTDIR}${PREFIX}/bin
mandir = ${DESTDIR}${MANPREFIX}/man1

all: ${PROG}

README: ${MAN}
	mandoc -I os=UNIX -T utf8 ${MAN} | col -b | expand -t 8 >$@

${PROG}: ${OBJS}
	${CC} -o $@ ${OBJS} ${LIBS} ${LDFLAGS}

.c.o:
	${CC} -std=c99 -pedantic ${DEFS} ${INCS} ${CFLAGS} ${CPPFLAGS} -o $@ -c $<

tags: ${SRCS}
	ctags ${SRCS}

lint: ${SRCS}
	-mandoc -T lint -W warning ${MAN}
	-clang-tidy ${SRCS} -- -std=c99 ${DEFS} ${INCS} ${CPPFLAGS}

clean:
	rm -f ${OBJS} ${PROG} ${PROG:=.core} tags

install: all
	mkdir -p ${bindir}
	mkdir -p ${mandir}
	install -m 755 ${PROG} ${bindir}/${PROG}
	install -m 644 ${MAN} ${mandir}/${MAN}

uninstall:
	-rm ${bindir}/${PROG}
	-rm ${mandir}/${MAN}

.PHONY: all tags clean install uninstall lint
