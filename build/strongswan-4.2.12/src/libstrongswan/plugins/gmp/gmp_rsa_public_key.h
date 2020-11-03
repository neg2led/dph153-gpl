/*
 * Copyright (C) 2005-2008 Martin Willi
 * Copyright (C) 2005 Jan Hutter
 * Hochschule fuer Technik Rapperswil
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * $Id: gmp_rsa_public_key.h 3721 2008-04-01 14:51:31Z martin $
 */

/**
 * @defgroup gmp_rsa_public_key gmp_rsa_public_key
 * @{ @ingroup gmp_p
 */

#ifndef GMP_RSA_PUBLIC_KEY_H_
#define GMP_RSA_PUBLIC_KEY_H_

typedef struct gmp_rsa_public_key_t gmp_rsa_public_key_t;

#include <credentials/keys/public_key.h>

/**
 * public_key_t implementation of RSA algorithm using libgmp.
 */
struct gmp_rsa_public_key_t {

	/**
	 * Implements the public_key_t interface
	 */
	public_key_t interface;
};

/**
 * Create the builder for a public key.
 *
 * @param type		type of the key, must be KEY_RSA
 * @return 			builder instance
 */
builder_t *gmp_rsa_public_key_builder(key_type_t type);

#endif /*GMP_RSA_PUBLIC_KEY_H_ @}*/
