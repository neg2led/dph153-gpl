/*
 * Copyright (C) 2005 Jan Hutter
 * Copyright (C) 2005-2006 Martin Willi
 *
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
 * $Id: hasher.h 4880 2009-02-18 19:45:46Z tobias $
 */
 
/**
 * @defgroup traffic_selector traffic_selector
 * @{ @ingroup config
 */

#ifndef HASHER_H_
#define HASHER_H_

typedef enum hash_algorithm_t hash_algorithm_t;
typedef struct hasher_t hasher_t;

#include <library.h>

/**
 * Algorithms to use for hashing.
 */
enum hash_algorithm_t {
	/** not specified hash function */
	HASH_UNKNOWN 		= 0,
	/** preferred hash function, general purpose */
	HASH_PREFERRED	 	= 1,
	HASH_MD2 			= 2,
	HASH_MD5 			= 3,
	HASH_SHA1 			= 4,
	HASH_SHA256 		= 5,
	HASH_SHA384 		= 6,
	HASH_SHA512 		= 7,
	HASH_MD4			= 8,
};

#define HASH_SIZE_MD2		16
#define HASH_SIZE_MD4		16
#define HASH_SIZE_MD5		16
#define HASH_SIZE_SHA1		20
#define HASH_SIZE_SHA256	32
#define HASH_SIZE_SHA384	48
#define HASH_SIZE_SHA512	64

/**
 * enum names for hash_algorithm_t.
 */
extern enum_name_t *hash_algorithm_names;

/**
 * Generic interface for all hash functions.
 */
struct hasher_t {
	/**
	 * Hash data and write it in the buffer.
	 * 
	 * If the parameter hash is NULL, no result is written back
	 * and more data can be appended to already hashed data.
	 * If not, the result is written back and the hasher is reset.
	 * 
	 * The hash output parameter must hold at least
	 * hash_t.get_block_size() bytes.
	 * 
	 * @param data		data to hash
	 * @param hash		pointer where the hash will be written
	 */
	void (*get_hash) (hasher_t *this, chunk_t data, u_int8_t *hash);
	
	/**
	 * Hash data and allocate space for the hash.
	 * 
	 * If the parameter hash is NULL, no result is written back
	 * and more data can be appended to already hashed data.
	 * If not, the result is written back and the hasher is reset.
	 * 
	 * @param data		chunk with data to hash
	 * @param hash		chunk which will hold allocated hash
	 */
	void (*allocate_hash) (hasher_t *this, chunk_t data, chunk_t *hash);
	
	/**
	 * Get the size of the resulting hash.
	 * 
	 * @return			hash size in bytes
	 */
	size_t (*get_hash_size) (hasher_t *this);
	
	/**
	 * Resets the hashers state.
	 */
	void (*reset) (hasher_t *this);
	
	/**
	 * Destroys a hasher object.
	 */
	void (*destroy) (hasher_t *this);
};

/**
 * Conversion of ASN.1 OID to hash algorithm.
 * 
 * @param oid			ASN.1 OID
 * @return				hash algorithm, HASH_UNKNOWN if OID unsuported
 */
hash_algorithm_t hasher_algorithm_from_oid(int oid);

/**
 * Conversion of hash algorithm into ASN.1 OID.
 * 
 * @param alg			hash algorithm
 * @return				ASN.1 OID, or OID_UNKNOW
 */
int hasher_algorithm_to_oid(hash_algorithm_t alg);

/**
 * Conversion of hash signature algorithm into ASN.1 OID.
 * 
 * @param alg			hash algorithm
 * @return				ASN.1 OID if, or OID_UNKNOW
 */
int hasher_signature_algorithm_to_oid(hash_algorithm_t alg);

#endif /* HASHER_H_ @} */
