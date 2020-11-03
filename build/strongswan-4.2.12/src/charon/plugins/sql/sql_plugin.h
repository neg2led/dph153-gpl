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
 * $Id: sql_plugin.h 3577 2008-03-12 14:38:35Z martin $
 */

/**
 * @defgroup sql sql
 * @ingroup cplugins
 *
 * @defgroup sql_plugin sql_plugin
 * @{ @ingroup sql
 */

#ifndef SQL_PLUGIN_H_
#define SQL_PLUGIN_H_

#include <plugins/plugin.h>

typedef struct sql_plugin_t sql_plugin_t;

/**
 * SQL database configuration plugin
 */
struct sql_plugin_t {

	/**
	 * implements plugin interface
	 */
	plugin_t plugin;
};

/**
 * Create a sql_plugin instance.
 */
plugin_t *plugin_create();

#endif /* SQL_PLUGIN_H_ @}*/
