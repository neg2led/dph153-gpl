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
 * $Id: ike_dpd.h 3589 2008-03-13 14:14:44Z martin $
 */

/**
 * @defgroup ike_dpd ike_dpd
 * @{ @ingroup tasks
 */

#ifndef IKE_DPD_H_
#define IKE_DPD_H_

typedef struct ike_dpd_t ike_dpd_t;

#include <library.h>
#include <sa/ike_sa.h>
#include <sa/tasks/task.h>

/**
 * Task of type ike_dpd, detects dead peers.
 *
 * The DPD task actually does nothing, as a DPD has no associated payloads.
 */
struct ike_dpd_t {

	/**
	 * Implements the task_t interface
	 */
	task_t task;
};

/**
 * Create a new ike_dpd task.
 *
 * @param initiator		TRUE if thask is the original initator
 * @return				ike_dpd task to handle by the task_manager
 */
ike_dpd_t *ike_dpd_create(bool initiator);

#endif /* IKE_DPD_H_ @} */
