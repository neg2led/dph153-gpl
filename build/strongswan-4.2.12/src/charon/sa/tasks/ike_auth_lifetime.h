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
 * $Id: ike_auth_lifetime.h 3589 2008-03-13 14:14:44Z martin $
 */

/**
 * @defgroup ike_auth_lifetime ike_auth_lifetime
 * @{ @ingroup tasks
 */

#ifndef IKE_AUTH_LIFETIME_H_
#define IKE_AUTH_LIFETIME_H_

typedef struct ike_auth_lifetime_t ike_auth_lifetime_t;

#include <library.h>
#include <sa/ike_sa.h>
#include <sa/tasks/task.h>

/**
 * Task of type IKE_AUTH_LIFETIME, implements RFC4478.
 *
 * This task exchanges lifetimes for IKE_AUTH to force a client to 
 * reauthenticate before the responders lifetime reaches the limit.
 */
struct ike_auth_lifetime_t {

	/**
	 * Implements the task_t interface
	 */
	task_t task;
};

/**
 * Create a new IKE_AUTH_LIFETIME task.
 *
 * @param ike_sa		IKE_SA this task works for
 * @param initiator		TRUE if taks is initiated by us
 * @return			  	ike_auth_lifetime task to handle by the task_manager
 */
ike_auth_lifetime_t *ike_auth_lifetime_create(ike_sa_t *ike_sa, bool initiator);

#endif /* IKE_MOBIKE_H_ @} */
