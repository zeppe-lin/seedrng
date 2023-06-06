//! \file  pathnames.h
//! \brief Absolute filenames that seedrng wants for various defaults.

#pragma once

#ifndef LOCALSTATEDIR
# define LOCALSTATEDIR          "/var/lib"
#endif

#define SEED_DIR LOCALSTATEDIR  "/seedrng"
#define CREDITABLE_SEED         "seed.credit"
#define NON_CREDITABLE_SEED     "seed.no-credit"

// vim:sw=2:ts=2:sts=2:et:cc=72:tw=70
// End of file.
