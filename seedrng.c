/*
 * SPDX-License-Identifier: (GPL-2.0 OR Apache-2.0 OR MIT OR BSD-1-Clause OR CC0-1.0)
 */

/*!
 * \file seedrng.c
 * \brief Seeds the Linux kernel random number generator from seed files.
 *
 * This program reads existing seed files, mixes their entropy into the
 * kernel's random number generator, and then saves a new seed for future boots.
 * It uses BLAKE2s for hashing to ensure entropy never decreases.
 *
 * \author Jason A. Donenfeld <Jason@zx2c4.com>
 * \author Alexandr Savca <alexandr.savca89@gmail.com> (Porting and Packaging)
 *
 * \copyright (C) 2022 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 */

#include <linux/random.h>
#include <sys/random.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <endian.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "pathnames.h"

/*! \brief Lengths used by the BLAKE2s hash function. */
enum blake2s_lengths {
	/*! \brief Block length in bytes. */
	BLAKE2S_BLOCK_LEN = 64,
	/*! \brief Hash output length in bytes. */
	BLAKE2S_HASH_LEN  = 32,
	/*! \brief Key length in bytes (not used in this program). */
	BLAKE2S_KEY_LEN   = 32
};

/*! \brief Lengths specific to the seedrng program. */
enum seedrng_lengths {
	/*! \brief Maximum allowed seed file length in bytes. */
	MAX_SEED_LEN = 512,
	/*! \brief Minimum allowed seed file length in bytes (equal to the BLAKE2s hash length). */
	MIN_SEED_LEN = BLAKE2S_HASH_LEN
};

/*! \brief Structure to hold the state of the BLAKE2s hash function. */
struct blake2s_state {
	uint32_t h[8]; /*!< Holds the hash state. */
	uint32_t t[2]; /*!< Holds the message counter. */
	uint32_t f[2]; /*!< Holds the finalization flags. */
	uint8_t buf[BLAKE2S_BLOCK_LEN]; /*!< Internal buffer for partial blocks. */
	unsigned int buflen; /*!< Number of bytes in the internal buffer. */
	unsigned int outlen; /*!< Desired output length of the hash. */
};

/*! \brief Converts a little-endian 32-bit value to CPU byte order. */
#define le32_to_cpup(a) le32toh(*(a))

/*! \brief Converts a 32-bit value in CPU byte order to little-endian. */
#define cpu_to_le32(a) htole32(a)

#ifndef ARRAY_SIZE
/*! \brief Calculates the number of elements in a static array. */
# define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

#ifndef DIV_ROUND_UP
/*! \brief Divides two numbers and rounds the result up to the nearest integer. */
# define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))
#endif

/**
 * @brief Converts an array of 32-bit unsigned integers from CPU endianness to little-endian.
 *
 * This function iterates through the provided array and converts the byte order
 * of each 32-bit integer to little-endian using the `cpu_to_le32` macro.
 *
 * @param buf A pointer to the first element of the array of 32-bit unsigned integers.
 * The conversion is done in-place.
 * @param words The number of 32-bit words in the array.
 */
static inline void cpu_to_le32_array(uint32_t *buf, unsigned int words)
{
	while (words--) {
		*buf = cpu_to_le32(*buf);
		++buf;
	}
}

/**
 * @brief Converts an array of 32-bit unsigned integers from little-endian to CPU endianness.
 *
 * This function iterates through the provided array and converts the byte order
 * of each 32-bit integer from little-endian to the CPU's native endianness
 * using the `le32_to_cpup` macro.
 *
 * @param buf A pointer to the first element of the array of 32-bit unsigned integers.
 * The conversion is done in-place.
 * @param words The number of 32-bit words in the array.
 */
static inline void le32_to_cpu_array(uint32_t *buf, unsigned int words)
{
	while (words--) {
		*buf = le32_to_cpup(buf);
		++buf;
	}
}

/**
 * @brief Performs a 32-bit right bitwise rotation.
 *
 * This function rotates the bits of a 32-bit unsigned integer to the right
 * by the specified number of positions. The shift amount is masked to ensure
 * it's within the valid range for a 32-bit word (0-31).
 *
 * @param word The 32-bit unsigned integer to rotate.
 * @param shift The number of bit positions to rotate to the right.
 * @return The result of the right bitwise rotation.
 */
static inline uint32_t ror32(uint32_t word, unsigned int shift)
{
	return (word >> (shift & 31)) | (word << ((-shift) & 31));
}

/**
 * @brief The initialization vector (IV) for the BLAKE2s hash function.
 *
 * This constant array contains the eight 32-bit unsigned integer values
 * that are used to initialize the internal state of the BLAKE2s hashing
 * algorithm. These values are part of the BLAKE2s specification.
 */
static const uint32_t blake2s_iv[8] = {
	0x6A09E667UL, 0xBB67AE85UL, 0x3C6EF372UL, 0xA54FF53AUL,
	0x510E527FUL, 0x9B05688CUL, 0x1F83D9ABUL, 0x5BE0CD19UL
};

/**
 * @brief The sigma permutation for the BLAKE2s hash function.
 *
 * This constant 2-dimensional array defines the order in which message words
 * are accessed during each of the 10 rounds of the BLAKE2s compression function.
 * Each row represents a round, and the 16 values in each row are a permutation
 * of the indices 0 to 15, indicating the order of message word selection.
 */
static const uint8_t blake2s_sigma[10][16] = {
	{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
	{ 14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3 },
	{ 11, 8, 12, 0, 5, 2, 15, 13, 10, 14, 3, 6, 7, 1, 9, 4 },
	{ 7, 9, 3, 1, 13, 12, 11, 14, 2, 6, 5, 10, 4, 0, 15, 8 },
	{ 9, 0, 5, 7, 2, 4, 10, 15, 14, 1, 11, 12, 6, 8, 3, 13 },
	{ 2, 12, 6, 10, 0, 11, 8, 3, 4, 13, 7, 5, 15, 14, 1, 9 },
	{ 12, 5, 1, 15, 14, 13, 4, 10, 0, 7, 6, 3, 9, 2, 8, 11 },
	{ 13, 11, 7, 14, 12, 1, 3, 9, 5, 0, 15, 4, 8, 6, 2, 10 },
	{ 6, 15, 14, 9, 11, 3, 0, 8, 12, 2, 13, 7, 1, 4, 10, 5 },
	{ 10, 2, 8, 4, 7, 6, 1, 5, 15, 11, 9, 14, 3, 12, 13, 0 },
};

/**
 * @brief Sets the last block flag in the BLAKE2s state.
 *
 * This function sets a flag in the BLAKE2s state structure to indicate
 * that the current block being processed is the last block of the input data.
 * This is typically done during the finalization phase of the hashing process.
 *
 * @param state A pointer to the BLAKE2s state structure.
 */
static void blake2s_set_lastblock(struct blake2s_state *state)
{
	state->f[0] = -1;
}

/**
 * @brief Increments the 64-bit counter in the BLAKE2s state.
 *
 * This function increments the 64-bit counter maintained within the BLAKE2s
 * state structure. The counter is represented by two 32-bit unsigned integers:
 * `state->t[0]` (lower 32 bits) and `state->t[1]` (upper 32 bits). The function
 * adds the given increment to the lower part and handles any potential carry
 * to the upper part. This counter is used by the BLAKE2s algorithm to keep
 * track of the number of input bytes processed.
 *
 * @param state A pointer to the BLAKE2s state structure.
 * @param inc The 32-bit unsigned integer value to increment the counter by.
 */
static void blake2s_increment_counter(struct blake2s_state *state, const uint32_t inc)
{
	state->t[0] += inc;
	state->t[1] += (state->t[0] < inc);
}

/**
 * @brief Initializes the BLAKE2s state structure with a parameter.
 *
 * This function initializes the internal state of the BLAKE2s hash function.
 * It first sets the entire state structure to zero. Then, it copies the
 * standard BLAKE2s initialization vector (IV) into the hash state (`state->h`).
 * Finally, it XORs the first element of the hash state (`state->h[0]`) with
 * the provided parameter. This function is used for initializing the hash
 * with specific configuration parameters, such as the desired output length.
 *
 * @param state A pointer to the BLAKE2s state structure to initialize.
 * @param param A 32-bit unsigned integer parameter used to customize the initialization.
 */
static void blake2s_init_param(struct blake2s_state *state, const uint32_t param)
{
	int i;

	memset(state, 0, sizeof(*state));
	for (i = 0; i < 8; ++i)
		state->h[i] = blake2s_iv[i];
	state->h[0] ^= param;
}

/**
 * @brief Initializes the BLAKE2s state structure for a given output length.
 *
 * This function initializes the internal state of the BLAKE2s hash function
 * to produce a hash of the specified length. It calls the lower-level
 * initialization function `blake2s_init_param()` with a parameter that
 * includes the desired output length. It also stores the output length
 * in the state structure.
 *
 * @param state A pointer to the BLAKE2s state structure to initialize.
 * @param outlen The desired length of the output hash in bytes.
 */
static void blake2s_init(struct blake2s_state *state, const size_t outlen)
{
	blake2s_init_param(state, 0x01010000 | outlen);
	state->outlen = outlen;
}

/**
 * @brief Compresses a block of data into the BLAKE2s hash state.
 *
 * This function performs the core compression operation of the BLAKE2s
 * algorithm. It processes a block of input data, updating the internal
 * hash state.
 *
 * @param state A pointer to the BLAKE2s state structure.
 * @param block A pointer to the input data block to compress.
 * @param nblocks The number of blocks to compress.
 * @param inc The amount to increment the counter by for each block.
 */
static void blake2s_compress(struct blake2s_state *state, const uint8_t *block, size_t nblocks, const uint32_t inc)
{
	uint32_t m[16];
	uint32_t v[16];
	int i;

	while (nblocks > 0) {
		blake2s_increment_counter(state, inc);
		memcpy(m, block, BLAKE2S_BLOCK_LEN);
		le32_to_cpu_array(m, ARRAY_SIZE(m));
		memcpy(v, state->h, 32);
		v[ 8] = blake2s_iv[0];
		v[ 9] = blake2s_iv[1];
		v[10] = blake2s_iv[2];
		v[11] = blake2s_iv[3];
		v[12] = blake2s_iv[4] ^ state->t[0];
		v[13] = blake2s_iv[5] ^ state->t[1];
		v[14] = blake2s_iv[6] ^ state->f[0];
		v[15] = blake2s_iv[7] ^ state->f[1];

// The BLAKE2s round function is defined by the ROUND macro,
// which in turn uses the G macro. These are expanded inline.
// The ROUND macro is called 10 times.
#define G(r, i, a, b, c, d) do {                  \
	a += b + m[blake2s_sigma[r][2 * i + 0]];  \
	d = ror32(d ^ a, 16);                     \
	c += d;                                   \
	b = ror32(b ^ c, 12);                     \
	a += b + m[blake2s_sigma[r][2 * i + 1]];  \
	d = ror32(d ^ a, 8);                      \
	c += d;                                   \
	b = ror32(b ^ c, 7);                      \
} while (0)

#define ROUND(r) do {                        \
	G(r, 0, v[0], v[ 4], v[ 8], v[12]);  \
	G(r, 1, v[1], v[ 5], v[ 9], v[13]);  \
	G(r, 2, v[2], v[ 6], v[10], v[14]);  \
	G(r, 3, v[3], v[ 7], v[11], v[15]);  \
	G(r, 4, v[0], v[ 5], v[10], v[15]);  \
	G(r, 5, v[1], v[ 6], v[11], v[12]);  \
	G(r, 6, v[2], v[ 7], v[ 8], v[13]);  \
	G(r, 7, v[3], v[ 4], v[ 9], v[14]);  \
} while (0)
		ROUND(0);
		ROUND(1);
		ROUND(2);
		ROUND(3);
		ROUND(4);
		ROUND(5);
		ROUND(6);
		ROUND(7);
		ROUND(8);
		ROUND(9);

#undef G
#undef ROUND

		for (i = 0; i < 8; ++i)
			state->h[i] ^= v[i] ^ v[i + 8];

		block += BLAKE2S_BLOCK_LEN;
		--nblocks;
	}
}

/**
 * @brief Updates the BLAKE2s hash with a new block of data.
 *
 * This function takes a chunk of input data and updates the internal state
 * of the BLAKE2s hash. It buffers the input and calls the compression
 * function (`blake2s_compress`) when a full block of data is accumulated.
 *
 * @param state A pointer to the BLAKE2s state structure.
 * @param inp A pointer to the input data to be hashed.
 * @param inlen The length of the input data in bytes.
 */
static void blake2s_update(struct blake2s_state *state, const void *inp, size_t inlen)
{
	const size_t fill = BLAKE2S_BLOCK_LEN - state->buflen;
	const uint8_t *in = inp;

	if (!inlen)
		return;
	if (inlen > fill) {
		memcpy(state->buf + state->buflen, in, fill);
		blake2s_compress(state, state->buf, 1, BLAKE2S_BLOCK_LEN);
		state->buflen = 0;
		in += fill;
		inlen -= fill;
	}
	if (inlen > BLAKE2S_BLOCK_LEN) {
		const size_t nblocks = DIV_ROUND_UP(inlen, BLAKE2S_BLOCK_LEN);
		blake2s_compress(state, in, nblocks - 1, BLAKE2S_BLOCK_LEN);
		in += BLAKE2S_BLOCK_LEN * (nblocks - 1);
		inlen -= BLAKE2S_BLOCK_LEN * (nblocks - 1);
	}
	memcpy(state->buf + state->buflen, in, inlen);
	state->buflen += inlen;
}

/**
 * @brief Finalizes the BLAKE2s hashing process and retrieves the hash.
 *
 * This function sets the last block flag, pads the internal buffer if necessary,
 * performs the final compression round, converts the hash state to little-endian,
 * and copies the resulting hash to the output buffer.
 *
 * @param state A pointer to the BLAKE2s state structure.
 * @param out A pointer to the buffer where the resulting hash will be written.
 */
static void blake2s_final(struct blake2s_state *state, uint8_t *out)
{
	blake2s_set_lastblock(state);
	memset(state->buf + state->buflen, 0, BLAKE2S_BLOCK_LEN - state->buflen);
	blake2s_compress(state, state->buf, 1, state->buflen);
	cpu_to_le32_array(state->h, ARRAY_SIZE(state->h));
	memcpy(out, state->h, state->outlen);
}

/**
 * @brief Reads the specified number of random bytes into the buffer, handling interruptions.
 *
 * This function repeatedly calls the `getrandom` system call until the requested
 * number of bytes is read into the provided buffer. It handles `EINTR` errors
 * by retrying the system call.
 *
 * @param buf A pointer to the buffer where the random bytes will be stored.
 * @param count The number of random bytes to read.
 * @param flags Flags to pass to the `getrandom` system call.
 * @return The total number of bytes successfully read, or -1 on error.
 */
static ssize_t getrandom_full(void *buf, size_t count, unsigned int flags)
{
	ssize_t ret, total = 0;
	uint8_t *p = buf;

	do {
		ret = getrandom(p, count, flags);
		if (ret < 0 && errno == EINTR)
			continue;
		else if (ret < 0)
			return ret;
		total += ret;
		p += ret;
		count -= ret;
	} while (count);

	return total;
}

/**
 * @brief Reads the specified number of bytes from a file descriptor into the buffer, handling interruptions and EOF.
 *
 * This function repeatedly calls the `read` system call until the requested
 * number of bytes is read into the provided buffer or the end of the file is reached.
 * It handles `EINTR` errors by retrying the system call.
 *
 * @param fd The file descriptor to read from.
 * @param buf A pointer to the buffer where the read bytes will be stored.
 * @param count The number of bytes to read.
 * @return The total number of bytes successfully read, or -1 on error. If the end of the file
 * is reached before reading the requested number of bytes, the function returns the
 * number of bytes read up to that point.
 */
static ssize_t read_full(int fd, void *buf, size_t count)
{
	ssize_t ret, total = 0;
	uint8_t *p = buf;

	do {
		ret = read(fd, p, count);
		if (ret < 0 && errno == EINTR)
			continue;
		else if (ret < 0)
			return ret;
		else if (ret == 0)
			break;
		total += ret;
		p += ret;
		count -= ret;
	} while (count);

	return total;
}

/**
 * @brief Writes the specified number of bytes from the buffer to a file descriptor, handling interruptions.
 *
 * This function repeatedly calls the `write` system call until the requested
 * number of bytes is written to the provided file descriptor. It handles `EINTR`
 * errors by retrying the system call.
 *
 * @param fd The file descriptor to write to.
 * @param buf A pointer to the buffer containing the bytes to write.
 * @param count The number of bytes to write.
 * @return The total number of bytes successfully written, or -1 on error.
 */
static ssize_t write_full(int fd, const void *buf, size_t count)
{
	ssize_t ret, total = 0;
	const uint8_t *p = buf;

	do {
		ret = write(fd, p, count);
		if (ret < 0 && errno == EINTR)
			continue;
		else if (ret < 0)
			return ret;
		total += ret;
		p += ret;
		count -= ret;
	} while (count);

	return total;
}

/**
 * @brief Determines the optimal seed length based on the kernel's entropy pool size.
 *
 * This function reads the value of `/proc/sys/kernel/random/poolsize` to determine
 * the kernel's entropy pool size in bits. It then converts this value to bytes
 * and returns it as the optimal seed length. If it cannot determine the pool size,
 * it falls back to a minimum seed length. The returned value is also clamped
 * within the defined minimum and maximum seed length limits.
 *
 * @return The optimal seed length in bytes.
 */
static size_t determine_optimal_seed_len(void)
{
	size_t ret = 0;
	char poolsize_str[11] = { 0 };
	int fd = open("/proc/sys/kernel/random/poolsize", O_RDONLY);

	if (fd < 0 || read_full(fd, poolsize_str, sizeof(poolsize_str) - 1) < 0) {
		perror("Unable to determine pool size, falling back to 256 bits");
		ret = MIN_SEED_LEN;
	} else
		ret = DIV_ROUND_UP(strtoul(poolsize_str, NULL, 10), 8);
	if (fd >= 0)
		close(fd);
	if (ret < MIN_SEED_LEN)
		ret = MIN_SEED_LEN;
	else if (ret > MAX_SEED_LEN)
		ret = MAX_SEED_LEN;

	return ret;
}

/**
 * @brief Reads new random data to be used as a seed.
 *
 * This function attempts to read new random data of the specified length.
 * It prioritizes using the `getrandom` system call with `GRND_NONBLOCK`
 * and `GRND_INSECURE` flags. As a fallback, it reads from `/dev/urandom`.
 *
 * @param seed A pointer to the buffer where the new seed will be stored.
 * @param len The desired length of the new seed in bytes.
 * @param is_creditable A pointer to a boolean that will be set to true if the
 * seed is considered creditable (obtained from a high-quality source),
 * false otherwise.
 * @return 0 on success, -1 on error (with errno set).
 */
static int read_new_seed(uint8_t *seed, size_t len, bool *is_creditable)
{
	ssize_t ret;
	int urandom_fd;

	*is_creditable = false;
	ret = getrandom_full(seed, len, GRND_NONBLOCK);
	if (ret == (ssize_t)len) {
		*is_creditable = true;
		return 0;
	} else if (ret < 0 && errno == ENOSYS) {
		struct pollfd random_fd = {
			.fd = open("/dev/random", O_RDONLY),
			.events = POLLIN
		};
		if (random_fd.fd < 0)
			return -errno;
		*is_creditable = poll(&random_fd, 1, 0) == 1;
		close(random_fd.fd);
	} else if (getrandom_full(seed, len, GRND_INSECURE) == (ssize_t)len)
		return 0;
	urandom_fd = open("/dev/urandom", O_RDONLY);
	if (urandom_fd < 0)
		return -1;
	ret = read_full(urandom_fd, seed, len);
	if (ret == (ssize_t)len)
		ret = 0;
	else
		ret = -errno ? -errno : -EIO;
	close(urandom_fd);
	errno = -ret;
	return ret ? -1 : 0;
}

/**
 * @brief Seeds the Linux kernel random number generator with the provided data.
 *
 * This function uses the `RNDADDENTROPY` ioctl on `/dev/urandom` to add
 * the provided seed data to the kernel's random number generator. The amount
 * of entropy added depends on the `credit` parameter.
 *
 * @param seed A pointer to the buffer containing the seed data.
 * @param len The length of the seed data in bytes.
 * @param credit A boolean indicating whether the seed is considered creditable.
 * If true, the entropy count passed to the kernel will be the length of the seed
 * in bits; otherwise, the entropy count will be 0.
 * @return 0 on success, -1 on error (with errno set).
 */
static int seed_rng(uint8_t *seed, size_t len, bool credit)
{
	struct {
		int entropy_count;
		int buf_size;
		uint8_t buffer[MAX_SEED_LEN];
	} req = {
		.entropy_count = credit ? len * 8 : 0,
		.buf_size = len
	};
	int random_fd, ret;

	if (len > sizeof(req.buffer)) {
		errno = EFBIG;
		return -1;
	}
	memcpy(req.buffer, seed, len);

	random_fd = open("/dev/urandom", O_RDONLY);
	if (random_fd < 0)
		return -1;
	ret = ioctl(random_fd, RNDADDENTROPY, &req);
	if (ret)
		ret = -errno ? -errno : -EIO;
	close(random_fd);
	errno = -ret;
	return ret ? -1 : 0;
}

/**
 * @brief Seeds the random number generator from a file if it exists.
 *
 * This function attempts to open and read a seed file with the specified
 * filename within the directory associated with the provided file descriptor.
 * If the file exists, its contents are read, the file is unlinked, and the
 * seed data is used to seed the kernel's random number generator. The length
 * and content of the seed are also added to the BLAKE2s hash.
 *
 * @param filename The name of the seed file to attempt to read.
 * @param dfd The file descriptor of the directory where the seed file is located.
 * @param credit A boolean indicating whether the seed read from the file should
 * be considered creditable when seeding the kernel.
 * @param hash A pointer to the BLAKE2s state structure to update with the seed data.
 * @return 0 if the file was successfully read and seeded, 0 if the file did not exist,
 * or -1 on error (with errno set).
 */
static int seed_from_file_if_exists(const char *filename, int dfd, bool credit, struct blake2s_state *hash)
{
	uint8_t seed[MAX_SEED_LEN];
	ssize_t seed_len;
	int fd = -1, ret = 0;

	fd = openat(dfd, filename, O_RDONLY);
	if (fd < 0 && errno == ENOENT)
		return 0;
	else if (fd < 0) {
		ret = -errno;
		perror("Unable to open seed file");
		goto out;
	}
	seed_len = read_full(fd, seed, sizeof(seed));
	if (seed_len < 0) {
		ret = -errno;
		perror("Unable to read seed file");
		goto out;
	}
	if ((unlinkat(dfd, filename, 0) < 0 || fsync(dfd) < 0) && seed_len) {
		ret = -errno;
		perror("Unable to remove seed after reading, so not seeding");
		goto out;
	}
	if (!seed_len)
		goto out;

	blake2s_update(hash, &seed_len, sizeof(seed_len));
	blake2s_update(hash, seed, seed_len);

	printf("Seeding %zd bits %s crediting\n",
		seed_len * 8,
		credit ? "and" : "without");

	if (seed_rng(seed, seed_len, credit) < 0) {
		ret = -errno;
		perror("Unable to seed");
	}

out:
	if (fd >= 0)
		close(fd);
	errno = -ret;
	return ret ? -1 : 0;
}

/**
 * @brief Checks if the crediting of new seeds should be skipped based on an environment variable.
 *
 * This function reads the value of the `SEEDRNG_SKIP_CREDIT` environment variable.
 * It returns `true` if the variable is set to "1", "true" (case-insensitive),
 * "yes" (case-insensitive), or "y" (case-insensitive). In all other cases (including
 * if the variable is not set), it returns `false`.
 *
 * @return `true` if crediting should be skipped, `false` otherwise.
 */
static bool skip_credit(void)
{
	const char *skip = getenv("SEEDRNG_SKIP_CREDIT");
	return skip && (!strcmp(skip, "1") || !strcasecmp(skip, "true") ||
			!strcasecmp(skip, "yes") || !strcasecmp(skip, "y"));
}

/**
 * @brief Main entry point of the seedrng program.
 *
 * This function initializes the BLAKE2s hash, seeds the kernel RNG from
 * existing seed files, generates a new seed, updates the hash with the
 * new seed, saves the new seed to a file (marking it as creditable or
 * non-creditable), and handles error conditions.
 *
 * @param argc The number of command-line arguments (unused).
 * @param argv An array of command-line arguments (unused).
 * @return The exit status of the program. 0 indicates success, non-zero indicates an error.
 * The lower bits of the return value can indicate specific errors:
 * - Bit 1: Error seeding from the non-creditable file.
 * - Bit 2: Error seeding from the creditable file.
 * - Bit 3: Error reading new seed data.
 * - Bit 4: Error opening seed file for writing.
 * - Bit 5: Error writing seed file.
 * - Bit 6: Error renaming seed file to make it creditable.
 */
int main(int argc __attribute__((unused)), char *argv[] __attribute__((unused)))
{
	static const char seedrng_prefix[] = "SeedRNG v1 Old+New Prefix";
	static const char seedrng_failure[] = "SeedRNG v1 No New Seed Failure";
	int fd = -1, dfd = -1, program_ret = 0;
	uint8_t new_seed[MAX_SEED_LEN];
	size_t new_seed_len;
	bool new_seed_creditable;
	struct timespec realtime = { 0 }, boottime = { 0 };
	struct blake2s_state hash;

	umask(0077);
	if (getuid()) {
		errno = EACCES;
		perror("This program requires root");
		return 1;
	}

	blake2s_init(&hash, BLAKE2S_HASH_LEN);
	blake2s_update(&hash, seedrng_prefix, strlen(seedrng_prefix));
	clock_gettime(CLOCK_REALTIME, &realtime);
	clock_gettime(CLOCK_BOOTTIME, &boottime);
	blake2s_update(&hash, &realtime, sizeof(realtime));
	blake2s_update(&hash, &boottime, sizeof(boottime));

	if (mkdir(SEED_DIR, 0700) < 0 && errno != EEXIST) {
		perror("Unable to create seed directory");
		return 1;
	}

	dfd = open(SEED_DIR, O_DIRECTORY | O_RDONLY);
	if (dfd < 0 || flock(dfd, LOCK_EX) < 0) {
		perror("Unable to lock seed directory");
		program_ret = 1;
		goto out;
	}

	if (seed_from_file_if_exists(NON_CREDITABLE_SEED, dfd, false, &hash) < 0)
		program_ret |= 1 << 1;
	if (seed_from_file_if_exists(CREDITABLE_SEED, dfd, !skip_credit(), &hash) < 0)
		program_ret |= 1 << 2;

	new_seed_len = determine_optimal_seed_len();
	if (read_new_seed(new_seed, new_seed_len, &new_seed_creditable) < 0) {
		perror("Unable to read new seed");
		new_seed_len = BLAKE2S_HASH_LEN;
		strncpy((char *)new_seed, seedrng_failure, new_seed_len);
		program_ret |= 1 << 3;
	}
	blake2s_update(&hash, &new_seed_len, sizeof(new_seed_len));
	blake2s_update(&hash, new_seed, new_seed_len);
	blake2s_final(&hash, new_seed + new_seed_len - BLAKE2S_HASH_LEN);

	printf("Saving %zu bits of %s seed for next boot\n",
		new_seed_len * 8,
		new_seed_creditable ? "creditable" : "non-creditable");

	fd = openat(dfd, NON_CREDITABLE_SEED, O_WRONLY | O_CREAT | O_TRUNC, 0400);
	if (fd < 0) {
		perror("Unable to open seed file for writing");
		program_ret |= 1 << 4;
		goto out;
	}
	if (write_full(fd, new_seed, new_seed_len) != (ssize_t)new_seed_len || fsync(fd) < 0) {
		perror("Unable to write seed file");
		program_ret |= 1 << 5;
		goto out;
	}
	if (new_seed_creditable && renameat(dfd, NON_CREDITABLE_SEED, dfd, CREDITABLE_SEED) < 0) {
		perror("Unable to make new seed creditable");
		program_ret |= 1 << 6;
	}
out:
	if (fd >= 0)
		close(fd);
	if (dfd >= 0)
		close(dfd);
	return program_ret;
}
