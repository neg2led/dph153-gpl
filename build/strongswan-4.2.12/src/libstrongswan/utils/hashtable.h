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
 * $Id: hashtable.h 4762 2008-12-05 10:01:52Z martin $
 */

/**
 * @defgroup hashtable hashtable
 * @{ @ingroup utils
 */

#ifndef HASHTABLE_H_
#define HASHTABLE_H_

#include <utils/enumerator.h>

typedef struct hashtable_t hashtable_t;

/**
 * Prototype for a function that computes the hash code from the given key.
 *
 * @param key			key to hash
 * @return				hash code
 */
typedef u_int (*hashtable_hash_t)(void *key);

/**
 * Prototype for a function that compares the two keys for equality.
 *
 * @param key			first key (the one we are looking for)
 * @param other_key		second key
 * @return				TRUE if the keys are equal
 */
typedef bool (*hashtable_equals_t)(void *key, void *other_key);

/**
 * Class implementing a hash table.
 *
 * General purpose hash table. This hash table is not synchronized.
 */
struct hashtable_t {
	
	/**
	 * Create an enumerator over the hash table key/value pairs.
	 * 
	 * @return			enumerator over (void *key, void *value)
	 */
	enumerator_t *(*create_enumerator) (hashtable_t *this);
	
	/**
	 * Adds the given value with the given key to the hash table, if there
	 * exists no entry with that key. NULL is returned in this case.
	 * Otherwise the existing value is replaced and the function returns the
	 * old value.
	 * 
	 * @param key		the key to store
	 * @param value		the value to store
	 * @return			NULL if no item was replaced, the old value otherwise
	 */
	void *(*put) (hashtable_t *this, void *key, void *value);
	
	/**
	 * Returns the value with the given key, if the hash table contains such an
	 * entry, otherwise NULL is returned.
	 * 
	 * @param key		the key of the requested value
	 * @return			the value, NULL if not found  
	 */
	void *(*get) (hashtable_t *this, void *key);
	
	/**
	 * Removes the value with the given key from the hash table and returns the
	 * removed value (or NULL if no such value existed).
	 * 
	 * @param key		the key of the value to remove
	 * @return			the removed value, NULL if not found
	 */
	void *(*remove) (hashtable_t *this, void *key);
	
	/**
	 * Gets the number of items in the hash table.
	 * 
	 * @return 			number of items
	 */
	u_int (*get_count) (hashtable_t *this);
	
	/**
	 * Destroys a hash table object.
	 */
	void (*destroy) (hashtable_t *this);
	
};

/**
 * Creates an empty hash table object.
 * 
 * @param hash			hash function
 * @param equals		equals function
 * @param capacity		initial capacity
 * @return 				hashtable_t object.
 */
hashtable_t *hashtable_create(hashtable_hash_t hash, hashtable_equals_t equals,
							  u_int capacity);

#endif /* HASHTABLE_H_ @} */
