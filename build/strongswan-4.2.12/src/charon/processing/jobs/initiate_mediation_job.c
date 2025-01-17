/*
 * Copyright (C) 2007-2008 Tobias Brunner
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
 * $Id: initiate_mediation_job.c 4625 2008-11-11 13:12:05Z tobias $
 */

#include "initiate_mediation_job.h"

#include <sa/ike_sa.h>
#include <daemon.h>


typedef struct private_initiate_mediation_job_t private_initiate_mediation_job_t;

/**
 * Private data of an initiate_mediation_job_t Object
 */
struct private_initiate_mediation_job_t {
	/**
	 * public initiate_mediation_job_t interface
	 */
	initiate_mediation_job_t public;
	
	/**
	 * ID of the IKE_SA of the mediated connection.
	 */
	ike_sa_id_t *mediated_sa_id;
	
	/**
	 * ID of the IKE_SA of the mediation connection.
	 */
	ike_sa_id_t *mediation_sa_id;
};

/**
 * Implements job_t.destroy.
 */
static void destroy(private_initiate_mediation_job_t *this)
{
	DESTROY_IF(this->mediation_sa_id);
	DESTROY_IF(this->mediated_sa_id);
	free(this);
}

/**
 * Callback to handle initiation of mediation connection
 */
static bool initiate_callback(private_initiate_mediation_job_t *this,
			debug_t group, level_t level, ike_sa_t *ike_sa,
			char *format, va_list args)
{
	if (ike_sa && !this->mediation_sa_id)
	{
		this->mediation_sa_id = ike_sa->get_id(ike_sa);
		this->mediation_sa_id = this->mediation_sa_id->clone(this->mediation_sa_id);
	}
	return TRUE;
}

/**
 * Implementation of job_t.execute.
 */ 
static void initiate(private_initiate_mediation_job_t *this)
{
	ike_sa_t *mediated_sa, *mediation_sa;
	peer_cfg_t *mediated_cfg, *mediation_cfg;
	
	mediated_sa = charon->ike_sa_manager->checkout(charon->ike_sa_manager,
												   this->mediated_sa_id);
	if (mediated_sa)
	{
		mediated_cfg = mediated_sa->get_peer_cfg(mediated_sa);
		mediated_cfg->get_ref(mediated_cfg); 
		
		charon->ike_sa_manager->checkin(charon->ike_sa_manager, mediated_sa);
		
		mediation_cfg = mediated_cfg->get_mediated_by(mediated_cfg);
		mediation_cfg->get_ref(mediation_cfg);
		
		if (charon->connect_manager->check_and_register(charon->connect_manager,
				mediation_cfg->get_my_id(mediation_cfg),
				mediated_cfg->get_peer_id(mediated_cfg),
				this->mediated_sa_id))
		{
			mediated_cfg->destroy(mediated_cfg);
			mediation_cfg->destroy(mediation_cfg);
			
			mediated_sa = charon->ike_sa_manager->checkout(
								charon->ike_sa_manager, this->mediated_sa_id);
			if (mediated_sa)
			{
				DBG1(DBG_IKE, "mediation with the same peer is already in "
					 "progress, queued");
				charon->ike_sa_manager->checkin(
								charon->ike_sa_manager, mediated_sa);
			}
			destroy(this);
			return;
		}
		/* we need an additional reference because initiate consumes one */
		mediation_cfg->get_ref(mediation_cfg); 

		if (charon->controller->initiate(charon->controller, mediation_cfg,
					NULL, (controller_cb_t)initiate_callback, this) != SUCCESS)
		{
			mediation_cfg->destroy(mediation_cfg);
			mediated_cfg->destroy(mediated_cfg);
			mediated_sa = charon->ike_sa_manager->checkout(
								charon->ike_sa_manager, this->mediated_sa_id);
			if (mediated_sa)
			{
				DBG1(DBG_IKE, "initiating mediation connection failed");
				charon->ike_sa_manager->checkin_and_destroy(
									charon->ike_sa_manager, mediated_sa);
			}
			destroy(this);
			return;
		}
		mediation_cfg->destroy(mediation_cfg);

		mediation_sa = charon->ike_sa_manager->checkout(charon->ike_sa_manager,
				this->mediation_sa_id);
		
		if (mediation_sa)
		{
			if (mediation_sa->initiate_mediation(mediation_sa,
												 mediated_cfg) != SUCCESS)
			{
				mediated_cfg->destroy(mediated_cfg);
				charon->ike_sa_manager->checkin_and_destroy(
								charon->ike_sa_manager, mediation_sa);
				mediated_sa = charon->ike_sa_manager->checkout(
								charon->ike_sa_manager, this->mediated_sa_id);
				if (mediated_sa)
				{
					DBG1(DBG_IKE, "establishing mediation connection failed");
					charon->ike_sa_manager->checkin_and_destroy(
										charon->ike_sa_manager, mediated_sa);
				}
				destroy(this);
				return;
			}
			
			charon->ike_sa_manager->checkin(charon->ike_sa_manager, mediation_sa);
		}
		
		mediated_cfg->destroy(mediated_cfg);
	}
	destroy(this);
}

/**
 * Implementation of job_t.execute.
 */ 
static void reinitiate(private_initiate_mediation_job_t *this)
{
	ike_sa_t *mediated_sa, *mediation_sa;
	peer_cfg_t *mediated_cfg;
	
	mediated_sa = charon->ike_sa_manager->checkout(charon->ike_sa_manager,
											  this->mediated_sa_id);
	if (mediated_sa)
	{
		mediated_cfg = mediated_sa->get_peer_cfg(mediated_sa);
		mediated_cfg->get_ref(mediated_cfg);
		charon->ike_sa_manager->checkin(charon->ike_sa_manager, mediated_sa);
		
		mediation_sa = charon->ike_sa_manager->checkout(charon->ike_sa_manager,
				this->mediation_sa_id);
		if (mediation_sa)
		{
			if (mediation_sa->initiate_mediation(mediation_sa, mediated_cfg) != SUCCESS)
			{
				DBG1(DBG_JOB, "initiating mediated connection '%s' failed",
						mediated_cfg->get_name(mediated_cfg));
				mediated_cfg->destroy(mediated_cfg);
				charon->ike_sa_manager->checkin_and_destroy(charon->ike_sa_manager, mediation_sa);
				mediated_sa = charon->ike_sa_manager->checkout(
								charon->ike_sa_manager, this->mediated_sa_id);
				if (mediated_sa)
				{
					DBG1(DBG_IKE, "establishing mediation connection failed");
					charon->ike_sa_manager->checkin_and_destroy(
										charon->ike_sa_manager, mediated_sa);
				}
				destroy(this);
				return;
			}
			charon->ike_sa_manager->checkin(charon->ike_sa_manager, mediation_sa);
		}
		
		mediated_cfg->destroy(mediated_cfg);
	}
	destroy(this);
}

/**
 * Creates an empty job
 */
static private_initiate_mediation_job_t *initiate_mediation_job_create_empty()
{
	private_initiate_mediation_job_t *this = malloc_thing(private_initiate_mediation_job_t);
	
	/* interface functions */
	this->public.job_interface.destroy = (void (*) (job_t *)) destroy;
	
	/* private variables */
	this->mediation_sa_id = NULL;
	this->mediated_sa_id = NULL;

	return this;
}

/*
 * Described in header
 */
initiate_mediation_job_t *initiate_mediation_job_create(ike_sa_id_t *ike_sa_id)
{
	private_initiate_mediation_job_t *this = initiate_mediation_job_create_empty();
	
	this->public.job_interface.execute = (void (*) (job_t *)) initiate;
	
	this->mediated_sa_id = ike_sa_id->clone(ike_sa_id);

	return &this->public;
}

/*
 * Described in header
 */
initiate_mediation_job_t *reinitiate_mediation_job_create(ike_sa_id_t *mediation_sa_id,
		ike_sa_id_t *mediated_sa_id)
{
	private_initiate_mediation_job_t *this = initiate_mediation_job_create_empty();
	
	this->public.job_interface.execute = (void (*) (job_t *)) reinitiate;
	
	this->mediation_sa_id = mediation_sa_id->clone(mediation_sa_id);
	this->mediated_sa_id = mediated_sa_id->clone(mediated_sa_id);
	
	return &this->public; 
}
