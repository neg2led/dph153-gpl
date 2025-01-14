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
 * $Id: kernel_netlink_plugin.h 4358 2008-09-25 13:56:23Z tobias $
 */

/**
 * @defgroup kernel_netlink kernel_netlink
 * @ingroup cplugins
 *
 * @defgroup kernel_netlink_plugin kernel_netlink_plugin
 * @{ @ingroup kernel_netlink
 */

#ifndef KERNEL_NETLINK_PLUGIN_H_
#define KERNEL_NETLINK_PLUGIN_H_

#include <plugins/plugin.h>

typedef struct kernel_netlink_plugin_t kernel_netlink_plugin_t;

/**
 * netlink kernel interface plugin
 */
struct kernel_netlink_plugin_t {

	/**
	 * implements plugin interface
	 */
	plugin_t plugin;
};

/**
 * Create a kernel_netlink_plugin instance.
 */
plugin_t *plugin_create();

#endif /* KERNEL_NETLINK_PLUGIN_H_ @} */
