# project metadata
NAME          = seedrng
VERSION       = 0.1
DIST          = ${NAME}-${VERSION}

# paths
PREFIX        = /usr
MANPREFIX     = ${PREFIX}/share/man
LOCALSTATEDIR = /var/lib

# flags
CFLAGS        = -pedantic -Wall -Wextra -DLOCALSTATEDIR=\"${LOCALSTATEDIR}\"
LDFLAGS       = -static
