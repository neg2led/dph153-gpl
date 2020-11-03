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
 * $Id: library.c 4311 2008-08-28 16:27:48Z martin $
 */

#include "library.h"

#include <stdlib.h>

#include <utils.h>
#include <chunk.h>
#include <utils/identification.h>
#include <utils/host.h>
#ifdef LEAK_DETECTIVE
#include <utils/leak_detective.h>
#endif

typedef struct private_library_t private_library_t;

/**
 * private data of library
 */
struct private_library_t {

	/**
	 * public functions
	 */
	library_t public;

#ifdef LEAK_DETECTIVE
	/**
	 * Memory leak detective, if enabled
	 */
	leak_detective_t *detective;
#endif /* LEAK_DETECTIVE */
};

/**
 * library instance
 */
library_t *lib;

/**
 * Implementation of library_t.destroy
 */
void library_deinit()
{
	private_library_t *this = (private_library_t*)lib;

	this->public.plugins->destroy(this->public.plugins);
	this->public.settings->destroy(this->public.settings);
	this->public.creds->destroy(this->public.creds);
	this->public.crypto->destroy(this->public.crypto);
	this->public.fetcher->destroy(this->public.fetcher);
	this->public.db->destroy(this->public.db);
	this->public.printf_hook->destroy(this->public.printf_hook);
	
#ifdef LEAK_DETECTIVE
	if (this->detective)
	{
		this->detective->destroy(this->detective);
	}
#endif /* LEAK_DETECTIVE */
	free(this);
	lib = NULL;
}

/*
 * see header file
 */
void library_init(char *settings)
{
	printf_hook_t *pfh;
	private_library_t *this = malloc_thing(private_library_t);
	lib = &this->public;
	
	lib->leak_detective = FALSE;
	
#ifdef LEAK_DETECTIVE
	this->detective = leak_detective_create();
#endif /* LEAK_DETECTIVE */

	pfh = printf_hook_create();
	this->public.printf_hook = pfh;
	
	pfh->add_handler(pfh, 'b', mem_get_printf_hooks());
	pfh->add_handler(pfh, 'B', chunk_get_printf_hooks());
	pfh->add_handler(pfh, 'D', identification_get_printf_hooks());
	pfh->add_handler(pfh, 'H', host_get_printf_hooks());
	pfh->add_handler(pfh, 'N', enum_get_printf_hooks());
	pfh->add_handler(pfh, 'T', time_get_printf_hooks());
	pfh->add_handler(pfh, 'V', time_delta_get_printf_hooks());
	
	this->public.crypto = crypto_factory_create();
	this->public.creds = credential_factory_create();
	this->public.fetcher = fetcher_manager_create();
	this->public.db = database_factory_create();
	this->public.settings = settings_create(settings);
	this->public.plugins = plugin_loader_create();
}

