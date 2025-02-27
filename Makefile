.POSIX:

include config.mk

all: seedrng

install: all
	mkdir -p ${DESTDIR}${PREFIX}/sbin
	mkdir -p ${DESTDIR}${MANPREFIX}/man8
	cp -f seedrng ${DESTDIR}${PREFIX}/sbin/
	cp -f seedrng.8 ${DESTDIR}${MANPREFIX}/man8/
	chmod 0755 ${DESTDIR}${PREFIX}/sbin/seedrng
	chmod 0644 ${DESTDIR}${MANPREFIX}/man8/seedrng.8

uninstall:
	rm -f ${DESTDIR}${PREFIX}/sbin/seedrng
	rm -f ${DESTDIR}${MANPREFIX}/man8/seedrng.8

clean:
	rm -f seedrng
	rm -f ${DIST}.tar.gz

release:
	git tag -a v${VERSION} -m v${VERSION}

dist: clean
	git archive --format=tar.gz -o ${DIST}.tar.gz --prefix=${DIST}/ HEAD

.PHONY: all install uninstall clean release dist
