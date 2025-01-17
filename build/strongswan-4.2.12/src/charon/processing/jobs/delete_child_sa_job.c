/*
 * Copyright (C) 2006 Martin Willi
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
 * $Id: delete_child_sa_job.c 3589 2008-03-13 14:14:44Z martin $
 */

#include "delete_child_sa_job.h"

#include <daemon.h>


typedef struct private_delete_child_sa_job_t private_delete_child_sa_job_t;

/**
 * Private data of an delete_child_sa_job_t object.
 */
struct private_delete_child_sa_job_t {
	/**

	 * Public delete_child_sa_job_t interface.
	 */
	delete_child_sa_job_t public;
	
	/**
	 * reqid of the CHILD_SA
	 */
	u_int32_t reqid;
	
	/**
	 * protocol of the CHILD_SA (ESP/AH)
	 */
	protocol_id_t protocol;
	
	/**
	 * inbound SPI of the CHILD_SA
	 */
	u_int32_t spi;
};

/**
 * Implementation of job_t.destroy.
 */
static void destroy(private_delete_child_sa_job_t *this)
{
	free(this);
}

/**
 * Implementation of job_t.execute.
 */
static void execute(private_delete_child_sa_job_t *this)
{
	ike_sa_t *ike_sa;
	
	ike_sa = charon->ike_sa_manager->checkout_by_id(charon->ike_sa_manager,
													this->reqid, TRUE);
	if (ike_sa == NULL)
	{
		DBG1(DBG_JOB, "CHILD_SA with reqid %d not found for delete",
			 this->reqid);
	}
	else
	{
		ike_sa->delete_child_sa(ike_sa, this->protocol, this->spi);
		
		charon->ike_sa_manager->checkin(charon->ike_sa_manager, ike_sa);
	}
	destroy(this);
}

/*
 * Described in header
 */
delete_child_sa_job_t *delete_child_sa_job_create(u_int32_t reqid, 
												  protocol_id_t protocol, 
												  u_int32_t spi)
{
	private_delete_child_sa_job_t *this = malloc_thing(private_delete_child_sa_job_t);
	
	/* interface functions */
	this->public.job_interface.execute = (void (*) (job_t *)) execute;
	this->public.job_interface.destroy = (void (*)(job_t*)) destroy;
	
	/* private variables */
	this->reqid = reqid;
	this->protocol = protocol;
	this->spi = spi;
	
	return &this->public;
}

