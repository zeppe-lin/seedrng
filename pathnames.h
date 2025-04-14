/*! \file  pathnames.h
 *  \brief Absolute filenames that seedrng wants for various defaults.
 */

#ifndef PATHNAMES_H
#define PATHNAMES_H

/*!< Directory where seed files are stored. */
#define SEED_DIR             LOCALSTATEDIR"/seedrng"

/*!< "Creditable" seed file. */
#define CREDITABLE_SEED      "seed.credit"

/*!< "Non-creditable" seed file. */
#define NON_CREDITABLE_SEED  "seed.no-credit"

#endif /* PATHNAMES_H */

/* End of file. */
