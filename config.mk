# project metadata
NAME = seedrng
VERSION = 0.1
DIST = ${NAME}-${VERSION}

# paths
PREFIX = /usr
MANPREFIX = ${PREFIX}/share/man
LOCALSTATEDIR = /var/lib

# flags
CPPFLAGS = -DLOCALSTATEDIR=\"${LOCALSTATEDIR}\"
CFLAGS   = -pedantic -Wall -Wextra
LDFLAGS  = -static

# compiler and linker
CC = cc
LD = ${CC}
