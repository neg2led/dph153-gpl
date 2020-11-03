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
 * $Id: eap_identity_plugin.h 3491 2008-02-22 14:04:00Z martin $
 */

/**
 * @defgroup eap_identity eap_identity
 * @ingroup cplugins
 *
 * @defgroup eap_identity_plugin eap_identity_plugin
 * @{ @ingroup eap_identity
 */

#ifndef EAP_IDENTITY_PLUGIN_H_
#define EAP_IDENTITY_PLUGIN_H_

#include <plugins/plugin.h>

typedef struct eap_identity_plugin_t eap_identity_plugin_t;

/**
 * EAP-IDENTITY plugin.
 */
struct eap_identity_plugin_t {

	/**
	 * implements plugin interface
	 */
	plugin_t plugin;
};

/**
 * Create a eap_identity_plugin instance.
 */
plugin_t *plugin_create();

#endif /* EAP_IDENTITY_PLUGIN_H_ @}*/
