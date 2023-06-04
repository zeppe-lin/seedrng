#pragma once

#ifndef LOCALSTATEDIR
# define LOCALSTATEDIR          "/var/lib"
#endif

#define SEED_DIR LOCALSTATEDIR  "/seedrng"
#define CREDITABLE_SEED         "seed.credit"
#define NON_CREDITABLE_SEED     "seed.no-credit"
