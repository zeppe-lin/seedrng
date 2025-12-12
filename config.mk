# project metadata
NAME          = seedrng
VERSION       = 0.3.0

# paths
PREFIX        = /usr
MANPREFIX     = ${PREFIX}/share/man
LOCALSTATEDIR = /var/lib

# flags
CPPFLAGS      = -D_DEFAULT_SOURCE -DLOCALSTATEDIR=\"${LOCALSTATEDIR}\"
CFLAGS        = -pedantic -Wall -Wextra -Wformat ${CPPFLAGS}
LDFLAGS       = -static
