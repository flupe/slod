PREFIX = /usr

all: slod

slod: slod.c
	${CC} -o $@ $< ${CFLAGS}

clean:
	rm slod

install: all
	mkdir -p ${DESDIR}${PREFIX}/bin
	cp -f slod ${DESTDIR}${PREFIX}/bin

uninstall:
	rm ${DESTDIR}${PREFIX}/bin/slod

.PHONY: all clean install uninstall
