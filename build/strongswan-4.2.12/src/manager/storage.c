/*
 * Copyright (C) 2007 Martin Willi
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
 * $Id: storage.c 3917 2008-05-08 13:12:43Z martin $
 */

#include "storage.h"

#include <library.h>
#include <crypto/hashers/hasher.h>


typedef struct private_storage_t private_storage_t;

/**
 * private data of storage
 */
struct private_storage_t {

	/**
	 * public functions
	 */
	storage_t public;
	
	/**
	 * database connection
	 */
	database_t *db;
};

/**
 * Implementation of storage_t.login.
 */
static int login(private_storage_t *this, char *username, char *password)
{
	hasher_t *hasher;
	chunk_t hash, data, hex_str;
	size_t username_len, password_len;
	int uid = 0;
	enumerator_t *enumerator;
	
	/* hash = SHA1( username | password ) */
	hasher = lib->crypto->create_hasher(lib->crypto, HASH_SHA1);
	if (hasher == NULL)
	{
		return 0;
	}
	hash = chunk_alloca(hasher->get_hash_size(hasher));
	username_len = strlen(username);
	password_len = strlen(password);
	data = chunk_alloca(username_len + password_len);
	memcpy(data.ptr, username, username_len);
	memcpy(data.ptr + username_len, password, password_len);
	hasher->get_hash(hasher, data, hash.ptr);
	hasher->destroy(hasher);
	hex_str = chunk_to_hex(hash, NULL, FALSE);
	
	enumerator = this->db->query(this->db, 
			"SELECT oid FROM users WHERE username = ? AND password = ?;",
			DB_TEXT, username, DB_TEXT, hex_str.ptr,
			DB_INT);
	if (enumerator)
	{
		enumerator->enumerate(enumerator, &uid);
		enumerator->destroy(enumerator);
	}
	free(hex_str.ptr);
	return uid;
}

/**
 * Implementation of storage_t.create_gateway_enumerator.
 */
static enumerator_t* create_gateway_enumerator(private_storage_t *this, int user)
{
	enumerator_t *enumerator;
	
	enumerator = this->db->query(this->db, 
			"SELECT gateways.oid AS gid, name, port, address FROM "
			"gateways, user_gateway AS ug ON gid = ug.gateway WHERE ug.user = ?;",
			DB_INT, user,
			DB_INT, DB_TEXT, DB_INT, DB_TEXT);
	if (!enumerator)
	{
		enumerator = enumerator_create_empty();
	}
	return enumerator;
}

/**
 * Implementation of storage_t.destroy
 */
static void destroy(private_storage_t *this)
{
	this->db->destroy(this->db);
	free(this);
}

/*
 * see header file
 */
storage_t *storage_create(char *uri)
{
	private_storage_t *this = malloc_thing(private_storage_t);
	
	this->public.login = (int(*)(storage_t*, char *username, char *password))login;
	this->public.create_gateway_enumerator = (enumerator_t*(*)(storage_t*,int))create_gateway_enumerator;
	this->public.destroy = (void(*)(storage_t*))destroy;
	
	this->db = lib->db->create(lib->db, uri);
	if (this->db == NULL)
	{
		free(this);
		return NULL;
	}
	return &this->public;
}

