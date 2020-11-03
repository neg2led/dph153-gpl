/*
 * Copyright (C) 2008 Tobias Brunner
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
 * $Id: openssl_ec_public_key.h 4051 2008-06-10 09:08:27Z tobias $
 */

/**
 * @defgroup openssl_ec_public_key openssl_ec_public_key
 * @{ @ingroup openssl_p
 */

#ifndef OPENSSL_EC_PUBLIC_KEY_H_
#define OPENSSL_EC_PUBLIC_KEY_H_

typedef struct openssl_ec_public_key_t openssl_ec_public_key_t;

#include <credentials/keys/public_key.h>

/**
 * public_key_t implementation of ECDSA using OpenSSL.
 */
struct openssl_ec_public_key_t {

	/**
	 * Implements the public_key_t interface
	 */
	public_key_t interface;
};

/**
 * Create the builder for a public key.
 *
 * @param type		type of the key, must be KEY_ECDSA
 * @return 			builder instance
 */
builder_t *openssl_ec_public_key_builder(key_type_t type);

#endif /*OPENSSL_EC_PUBLIC_KEY_H_ @}*/
