# project metadata
NAME          = seedrng
VERSION       = 0.1
DIST          = ${NAME}-${VERSION}

# paths
PREFIX        = /usr
MANPREFIX     = ${PREFIX}/share/man
LOCALSTATEDIR = /var/lib

# flags
CFLAGS        = -pedantic -Wall -Wextra \
		-D_DEFAULT_SOURCE -DLOCALSTATEDIR=\"${LOCALSTATEDIR}\"
LDFLAGS       = -static
