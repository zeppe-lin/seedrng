OVERVIEW
--------
This directory contains seedrng, a simple program for seeding the
Linux kernel random number generator from seed files.

This distribution is a fork of
[Jason A. Donenfeld](mailto:Jason@zx2c4.com)'s seedrng as of commit
f68fee4 (Wed Apr 20 02:43:45 2022 +0200) with the following
differences:
- add seedrng.8 manual page
- move paths from seedrng.c to pathnames.h
- adjust build to suckless style

See git log for complete/further differences.

The original sources can be downloaded from:
1. git://git.zx2c4.com/seedrng    (git)
2. https://git.zx2c4.com/seedrng  (web)


REQUIREMENTS
------------
**Build time**:
- c99 compiler
- POSIX sh(1p), make(1p) and "mandatory utilities"
- pod2man(1pm) to build man page


INSTALL
-------
SeedRNG is meant to be used by init system projects.
**Please do not use SeedRNG as a standalone program**.

The shell commands `make && make install` should build and install
this package.  See `config.mk` file for configuration parameters, and
`pathnames.h` for absolute filenames that SeedRNG wants for various
defaults.


USAGE
-----
```sh
# seedrng
```

However, this invocation should generally come from init and shutdown
scripts.


LICENSE
-------
This program is licensed under any one of the following licenses, so
that it can be incorporated into other software projects as needed:

- GPL-2.0
- Apache-2.0
- MIT
- BSD-1-Clause
- CC0-1.0
