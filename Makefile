.POSIX:

include config.mk

all: seedrng seedrng.8

seedrng.8: seedrng.8.scdoc
	scdoc < seedrng.8.scdoc > $@

install: all
	install -Dm 0755 seedrng ${DESTDIR}${PREFIX}/sbin/seedrng
	install -Dm 0644 seedrng.8 ${DESTDIR}${MANPREFIX}/man8/seedrng.8

uninstall:
	rm -f ${DESTDIR}${PREFIX}/sbin/seedrng
	rm -f ${DESTDIR}${MANPREFIX}/man8/seedrng.8

clean:
	rm -f seedrng seedrng.8

release:
	git tag -a v${VERSION} -m v${VERSION}

.PHONY: all install uninstall clean release
