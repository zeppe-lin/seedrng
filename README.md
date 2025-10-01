OVERVIEW
========

The `seedrng` utility seeds the Linux kernel random number generator
from stored seed files.

It is a fork of [Jason A. Donenfeld][1]'s SeedRNG at commit `f68fee4`
(Wed Apr 20 2022) with the following changes:
  * Added `seedrng(8)` manual page in `scdoc(5)` format
  * Moved paths definitions to `pathnames.h`
  * Added code documentation and technical notes
  * Simplified (suckless-style) build system

[1]: mailto:Jason@zx2c4.com

See git log for full history.

Original sources:
  * git://git.zx2c4.com/seedrng    (git)
  * https://git.zx2c4.com/seedrng  (web)


REQUIREMENTS
============

Build-time
----------
  * C99 compiler
  * POSIX `sh(1p)`, `make(1p)` and "mandatory utilities"
  * Linux kernel headers
  * `scdoc(1)` (to build manual pages)

Runtime
-------
  * Linux kernel with `/dev/random` and `/dev/urandom` :)


INSTALLATION
============

`seedrng` is intended for init system integration.
**Do not use manually as a standalone tool**.

Build and install:

    make && make install

Configuration parameters: `config.mk`

Default paths: `pathnames.h`


DOCUMENTATION
=============

Usage
-----

Run as root (normally via init/shutdown scripts):

    seedrng

Manual pages
------------

See `seedrng.8.scdoc`.

Technical Notes
---------------

### Entropy Hashing

To ensure that the entropy in the seed files either stays the same or
increases over time, `seedrng` employs the **BLAKE2s** cryptographic
hash function (with a 32-byte output) when creating new seed files.
The process involves hashing the following data:

    HASH(    "SeedRNG v1 Old+New Prefix"
            || current_real_time
            || system_boot_time
            || length_of_old_seed
            || old_seed_content
            || length_of_new_seed_data
            || new_seed_data
        )

The resulting 32-byte hash is then appended to the newly generated
random data to form the complete new seed.  Specifically, if
`new_seed` represents the newly generated random data of a certain
length, the final new seed stored to disk is constructed as:

    final_new_seed = new_seed[:-32] || BLAKE2s_HASH(...)

Where:
* `||` denotes concatenation.
* `BLAKE2s_HASH(...)` represents the 32-byte **BLAKE2s** hash of the
  concatenated data described above.
* `new_seed[:-32]` represents the initial portion of the newly
  generated random data, with the last 32 bytes reserved for the hash.

This design ensures that the new seed incorporates information about
the previous seed, the current system time, the boot time, and the
newly generated random data itself, enhancing its robustness and
preventing entropy loss.

### Path Declarations

The absolute file paths used by `seedrng` for storing the creditable
seed (`seed.credit`) and the non-creditable seed (`seed.no-credit`)
are defined as constants in the `pathnames.h` header file.  The
default location for these files is within the `/var/lib/seedrng/`
directory.


LICENSE
=======

`seedrng` may be used under any of the following licenses:
  * GPL-2.0
  * Apache-2.0
  * MIT
  * BSD-1-Clause
  * CC0-1.0
