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
 * $Id$
 */

#include "pubkey_plugin.h"

#include <library.h>
#include "pubkey_cert.h"
#include "pubkey_public_key.h"

typedef struct private_pubkey_plugin_t private_pubkey_plugin_t;

/**
 * private data of pubkey_plugin
 */
struct private_pubkey_plugin_t {

	/**
	 * public functions
	 */
	pubkey_plugin_t public;
};

/**
 * Implementation of pubkey_plugin_t.pubkeytroy
 */
static void destroy(private_pubkey_plugin_t *this)
{
	lib->creds->remove_builder(lib->creds,
							(builder_constructor_t)pubkey_cert_builder);
	lib->creds->remove_builder(lib->creds,
							(builder_constructor_t)pubkey_public_key_builder);
	free(this);
}

/*
 * see header file
 */
plugin_t *plugin_create()
{
	private_pubkey_plugin_t *this = malloc_thing(private_pubkey_plugin_t);
	
	this->public.plugin.destroy = (void(*)(plugin_t*))destroy;

	lib->creds->add_builder(lib->creds, CRED_CERTIFICATE, CERT_TRUSTED_PUBKEY,
							(builder_constructor_t)pubkey_cert_builder);
	lib->creds->add_builder(lib->creds, CRED_PUBLIC_KEY, KEY_ANY,
							(builder_constructor_t)pubkey_public_key_builder);

	return &this->public.plugin;
}

