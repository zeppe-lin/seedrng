# project metadata
NAME          = seedrng
VERSION       = 0.2.1
DIST          = ${NAME}-${VERSION}

# paths
PREFIX        = /usr
MANPREFIX     = ${PREFIX}/share/man
LOCALSTATEDIR = /var/lib

# flags
CPPFLAGS      = -D_DEFAULT_SOURCE -DLOCALSTATEDIR=\"${LOCALSTATEDIR}\"
CFLAGS        = -pedantic -Wall -Wextra ${CPPFLAGS}
LDFLAGS       = -static
