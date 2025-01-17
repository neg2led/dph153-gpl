/*
 * Copyright (C) 2008 Martin Willi
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
 * $Id: sha1_plugin.c 4308 2008-08-28 10:57:24Z martin $
 */

#include "sha1_plugin.h"

#include <library.h>
#include "sha1_hasher.h"
#include "sha1_prf.h"

typedef struct private_sha1_plugin_t private_sha1_plugin_t;

/**
 * private data of sha1_plugin
 */
struct private_sha1_plugin_t {

	/**
	 * public functions
	 */
	sha1_plugin_t public;
};

/**
 * Implementation of sha1_plugin_t.destroy
 */
static void destroy(private_sha1_plugin_t *this)
{
	lib->crypto->remove_hasher(lib->crypto,
							   (hasher_constructor_t)sha1_hasher_create);
	lib->crypto->remove_prf(lib->crypto,
							   (prf_constructor_t)sha1_prf_create);
	free(this);
}

/*
 * see header file
 */
plugin_t *plugin_create()
{
	private_sha1_plugin_t *this = malloc_thing(private_sha1_plugin_t);
	
	this->public.plugin.destroy = (void(*)(plugin_t*))destroy;
	
	lib->crypto->add_hasher(lib->crypto, HASH_SHA1,
							(hasher_constructor_t)sha1_hasher_create);
	lib->crypto->add_prf(lib->crypto, PRF_KEYED_SHA1,
							(prf_constructor_t)sha1_prf_create);
	
	return &this->public.plugin;
}

