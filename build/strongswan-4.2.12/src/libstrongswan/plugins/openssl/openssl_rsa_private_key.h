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
 * $Id: openssl_rsa_private_key.h 4000 2008-05-22 12:13:10Z tobias $
 */

/**
 * @defgroup openssl_rsa_private_key openssl_rsa_private_key
 * @{ @ingroup openssl_p
 */

#ifndef OPENSSL_RSA_PRIVATE_KEY_H_
#define OPENSSL_RSA_PRIVATE_KEY_H_

#include <credentials/keys/private_key.h>

typedef struct openssl_rsa_private_key_t openssl_rsa_private_key_t;

/**
 * private_key_t implementation of RSA algorithm using OpenSSL.
 */
struct openssl_rsa_private_key_t {

	/**
	 * Implements private_key_t interface
	 */
	private_key_t interface;
};

/**
 * Create the builder for a private key.
 *
 * @param type		type of the key, must be KEY_RSA
 * @return 			builder instance
 */
builder_t *openssl_rsa_private_key_builder(key_type_t type);

#endif /*OPENSSL_RSA_PRIVATE_KEY_H_ @}*/
