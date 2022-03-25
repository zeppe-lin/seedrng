PREFIX ?= /usr
DESTDIR ?=
SBINDIR ?= $(PREFIX)/sbin
LOCALSTATEDIR ?= /var/lib
RUNSTATEDIR ?= /var/run
CFLAGS ?= -O2 -pipe

CFLAGS += -Wall -Wextra -pedantic
CFLAGS += -DLOCALSTATEDIR="\"$(LOCALSTATEDIR)\"" -DRUNSTATEDIR="\"$(RUNSTATEDIR)\""

seedrng: seedrng.c

install: seedrng
	install -v -d "$(DESTDIR)$(SBINDIR)" && install -v -m 0755 seedrng "$(DESTDIR)$(SBINDIR)/seedrng"

clean:
	rm -f seedrng

.PHONY: clean
