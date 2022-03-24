## SeedRNG &mdash; `seedrng(8)`
##### by [Jason A. Donenfeld](mailto:Jason@zx2c4.com)

SeedRNG is a simple program made for seeding the Linux kernel random number
generator from seed files. The program takes no arguments, must be run as root,
and always attempts to do something useful.

Being a single C file, `seedrng.c`, SeedRNG is meant to be copy and pasted
verbatim into various minimal init system projects and tweaked as needed.

This program is useful in light of the fact that the Linux kernel RNG cannot be
initialized from shell scripts, and new seeds cannot be safely generated from
boot time shell scripts either.

It should be run once at init time and once at shutdown time. It can be run at
other times without detriment as well. Whenever it is run, it writes existing
seed files into the RNG pool, and then creates a new seed file. If the RNG is
initialized at the time of creating a new seed file, then that new seed file is
marked as "creditable", which means it can be used to initialize the RNG.
Otherwise, it is marked as "non-creditable", in which case it is still used to
seed the RNG's pool, but will not initialize the RNG.

In order to ensure that entropy only ever stays the same or increases from one
seed file to the next, old seed values are hashed together with new seed values
when writing new seed files:

```
new_seed = new_seed[:-32] || HASH(fixed_prefix || real_time || boot_time || old_seed_len || old_seed || new_seed_len || new_seed)
```

The seed is stored in `LOCALSTATEDIR/seedrng/`, which can be adjusted at compile time.

### Building &amp; Installing

```
$ make
$ sudo make install
```

In addition to the usual compiler environment variables (`CFLAGS`, etc), the
following environment variable is respected during compilation:

  * `LOCALSTATEDIR`        default: `/var/lib`
  * `RUNSTATEDIR`          default: `/var/run`

The following environment variables are respected during installation:

  * `PREFIX`               default: `/usr`
  * `DESTDIR`              default:
  * `SBINDIR`              default: `$(PREFIX)/sbin`

### Usage

```
# seedrng
```

However, this invocation should generally come from init and shutdown scripts.

### License

This program is licensed under any one of the GPL v2, MIT, or Apache 2.0, so
that it can be incorporated into other software projects as needed.
