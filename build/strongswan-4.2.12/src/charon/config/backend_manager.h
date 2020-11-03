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
 * $Id: backend_manager.h 4132 2008-07-01 09:05:20Z martin $
 */

/**
 * @defgroup backend_manager backend_manager
 * @{ @ingroup config
 */

#ifndef BACKEND_MANAGER_H_
#define BACKEND_MANAGER_H_

typedef struct backend_manager_t backend_manager_t;

#include <library.h>
#include <utils/host.h>
#include <utils/identification.h>
#include <config/ike_cfg.h>
#include <config/peer_cfg.h>
#include <config/backend.h>


/**
 * A loader and multiplexer to use multiple backends.
 *
 * Charon allows the use of multiple configuration backends simultaneously. To
 * access all this backends by a single call, this class wraps multiple
 * backends behind a single object.
 * @verbatim

   +---------+      +-----------+         +--------------+     |
   |         |      |           |       +--------------+ |     |
   | daemon  |----->| backend_- |     +--------------+ |-+  <==|==> IPC
   |  core   |      | manager   |---->|   backends   |-+       |
   |         |----->|           |     +--------------+         |
   |         |      |           |                              |
   +---------+      +-----------+                              |
   
   @endverbatim
 */
struct backend_manager_t {
	
	/**
	 * Get an ike_config identified by two hosts.
	 *
	 * @param my_host			address of own host
	 * @param other_host		address of remote host
	 * @return					matching ike_config, or NULL if none found
	 */
	ike_cfg_t* (*get_ike_cfg)(backend_manager_t *this, 
							  host_t *my_host, host_t *other_host);
	
	/**
	 * Get a peer_config identified by two IDs and authorization info.
	 *
	 * @param me				own address
	 * @param other				peer address
	 * @param my_id				own ID
	 * @param other_id			peer ID
	 * @param auth_info			authorization info
	 * @return					matching peer_config, or NULL if none found
	 */
	peer_cfg_t* (*get_peer_cfg)(backend_manager_t *this, host_t *me,
								host_t *other, identification_t *my_id,
								identification_t *other_id, auth_info_t *auth);
	
	/**
	 * Get a peer_config identified by it's name.
	 *
	 * @param name				name of the peer_config
	 * @return					matching peer_config, or NULL if none found
	 */
	peer_cfg_t* (*get_peer_cfg_by_name)(backend_manager_t *this, char *name);
	
	/**
	 * Create an enumerator over all peer configs.
	 *
	 * @return 					enumerator over peer configs
	 */
	enumerator_t* (*create_peer_cfg_enumerator)(backend_manager_t *this);
	
	/**
	 * Register a backend on the manager.
	 *
	 * @param backend			backend to register
	 */
	void (*add_backend)(backend_manager_t *this, backend_t *backend);
	
	/**
	 * Unregister a backend.
	 *
	 * @param backend			backend to unregister
	 */
	void (*remove_backend)(backend_manager_t *this, backend_t *backend);
	
	/**
	 * Destroys a backend_manager_t object.
	 */
	void (*destroy) (backend_manager_t *this);
};

/**
 * Create an instance of the backend manager
 *
 * @return		backend_manager instance
 */
backend_manager_t* backend_manager_create(void);

#endif /*BACKEND_MANAGER_H_ @} */

