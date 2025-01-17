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

#define _GNU_SOURCE
#include <string.h>

#include "medcli_config.h"

#include <daemon.h>
#include <processing/jobs/callback_job.h>

typedef struct private_medcli_config_t private_medcli_config_t;

/**
 * Private data of an medcli_config_t object
 */
struct private_medcli_config_t {

	/**
	 * Public part
	 */
	medcli_config_t public;
	
	/**
	 * database connection
	 */
	database_t *db;
	
	/**
	 * rekey time
	 */
	int rekey;
	
	/**
	 * dpd delay
	 */
	int dpd;
	
	/**
	 * default ike config
	 */
	ike_cfg_t *ike;
};

/**
 * create a traffic selector from a CIDR notation string
 */
static traffic_selector_t *ts_from_string(char *str)
{
	if (str)
	{
		int netbits = 32;
		host_t *net;
		char *pos;
		
		str = strdupa(str);
		pos = strchr(str, '/');
		if (pos)
		{
			*pos++ = '\0';
			netbits = atoi(pos);
		}
		else
		{
			if (strchr(str, ':'))
			{
				netbits = 128;
			}
		}
		net = host_create_from_string(str, 0);
		if (net)
		{
			return traffic_selector_create_from_subnet(net, netbits, 0, 0);
		}
	}
	return traffic_selector_create_dynamic(0, 0, 65535);
}

/**
 * implements backend_t.get_peer_cfg_by_name.
 */
static peer_cfg_t *get_peer_cfg_by_name(private_medcli_config_t *this, char *name)
{
	enumerator_t *e;
	peer_cfg_t *peer_cfg, *med_cfg;
	ike_cfg_t *ike_cfg;
	child_cfg_t *child_cfg;
	chunk_t me, other;
	char *address, *local_net, *remote_net;
	
	/* query mediation server config:
	 * - build ike_cfg/peer_cfg for mediation connection on-the-fly 
	 */
	e = this->db->query(this->db,
			"SELECT Address, ClientConfig.KeyId, MediationServerConfig.KeyId "
			"FROM MediationServerConfig JOIN ClientConfig",
			DB_TEXT, DB_BLOB, DB_BLOB);
	if (!e || !e->enumerate(e, &address, &me, &other))
	{
		DESTROY_IF(e);
		return NULL;
	}
	ike_cfg = ike_cfg_create(FALSE, FALSE, "0.0.0.0", address);
	ike_cfg->add_proposal(ike_cfg, proposal_create_default(PROTO_IKE));
	med_cfg = peer_cfg_create(
		"mediation", 2, ike_cfg,
		identification_create_from_encoding(ID_KEY_ID, me),
		identification_create_from_encoding(ID_KEY_ID, other),
		CERT_NEVER_SEND, UNIQUE_REPLACE, 
		1, this->rekey*60, 0,  			/* keytries, rekey, reauth */
		this->rekey*5, this->rekey*3, 	/* jitter, overtime */
		TRUE, this->dpd, 				/* mobike, dpddelay */
		NULL, NULL, 					/* vip, pool */
		TRUE, NULL, NULL); 				/* mediation, med by, peer id */
	e->destroy(e);
	
	/* query mediated config:
	 * - use any-any ike_cfg
	 * - build peer_cfg on-the-fly using med_cfg
	 * - add a child_cfg
	 */
	e = this->db->query(this->db,
			"SELECT ClientConfig.KeyId, Connection.KeyId, "
			"Connection.LocalSubnet, Connection.RemoteSubnet "
			"FROM ClientConfig JOIN Connection "
			"WHERE Active AND Alias = ?", DB_TEXT, name,
			DB_BLOB, DB_BLOB, DB_TEXT, DB_TEXT);
	if (!e || !e->enumerate(e, &me, &other, &local_net, &remote_net))
	{
		DESTROY_IF(e);
		return NULL;
	}
	peer_cfg = peer_cfg_create(
		name, 2, this->ike->get_ref(this->ike),
		identification_create_from_encoding(ID_KEY_ID, me),
		identification_create_from_encoding(ID_KEY_ID, other),
		CERT_NEVER_SEND, UNIQUE_REPLACE, 
		1, this->rekey*60, 0,  			/* keytries, rekey, reauth */
		this->rekey*5, this->rekey*3, 	/* jitter, overtime */
		TRUE, this->dpd, 				/* mobike, dpddelay */
		NULL, NULL, 					/* vip, pool */
		FALSE, med_cfg,				 	/* mediation, med by */
		identification_create_from_encoding(ID_KEY_ID, other));
	
	child_cfg = child_cfg_create(name, this->rekey*60 + this->rekey,
							  this->rekey*60, this->rekey, NULL, TRUE,
							  MODE_TUNNEL, ACTION_NONE, ACTION_NONE, FALSE);
	child_cfg->add_proposal(child_cfg, proposal_create_default(PROTO_ESP));
	child_cfg->add_traffic_selector(child_cfg, TRUE, ts_from_string(local_net));
	child_cfg->add_traffic_selector(child_cfg, FALSE, ts_from_string(remote_net));
	peer_cfg->add_child_cfg(peer_cfg, child_cfg);
	e->destroy(e);
	return peer_cfg;
}

/**
 * Implementation of backend_t.create_ike_cfg_enumerator.
 */
static enumerator_t* create_ike_cfg_enumerator(private_medcli_config_t *this,
											   host_t *me, host_t *other)
{
	return enumerator_create_single(this->ike, NULL);
}

typedef struct {
	/** implements enumerator */
	enumerator_t public;
	/** inner SQL enumerator */
	enumerator_t *inner;
	/** currently enumerated peer config */
	peer_cfg_t *current;
	/** ike cfg to use in peer cfg */
	ike_cfg_t *ike;
	/** rekey time */
	int rekey;
	/** dpd time */
	int dpd;
} peer_enumerator_t;

/**
 * Implementation of peer_enumerator_t.public.enumerate
 */
static bool peer_enumerator_enumerate(peer_enumerator_t *this, peer_cfg_t **cfg)
{
	char *name, *local_net, *remote_net;
	chunk_t me, other;
	child_cfg_t *child_cfg;

	DESTROY_IF(this->current);
	if (!this->inner->enumerate(this->inner, &name, &me, &other,
								&local_net, &remote_net))
	{
		this->current = NULL;
		return FALSE;
	}
	this->current = peer_cfg_create(
				name, 2, this->ike->get_ref(this->ike),
				identification_create_from_encoding(ID_KEY_ID, me),
				identification_create_from_encoding(ID_KEY_ID, other),
				CERT_NEVER_SEND, UNIQUE_REPLACE, 
				1, this->rekey*60, 0,  			/* keytries, rekey, reauth */
				this->rekey*5, this->rekey*3, 	/* jitter, overtime */
				TRUE, this->dpd, 				/* mobike, dpddelay */
				NULL, NULL, 					/* vip, pool */
				FALSE, NULL, NULL); 			/* mediation, med by, peer id */
	child_cfg = child_cfg_create(
				name, this->rekey*60 + this->rekey,
				this->rekey*60, this->rekey, NULL, TRUE,
				MODE_TUNNEL, ACTION_NONE, ACTION_NONE, FALSE);
	child_cfg->add_proposal(child_cfg, proposal_create_default(PROTO_ESP));
	child_cfg->add_traffic_selector(child_cfg, TRUE, ts_from_string(local_net));
	child_cfg->add_traffic_selector(child_cfg, FALSE, ts_from_string(remote_net));
	this->current->add_child_cfg(this->current, child_cfg);
	*cfg = this->current;
	return TRUE;
}

/**
 * Implementation of peer_enumerator_t.public.destroy
 */
static void peer_enumerator_destroy(peer_enumerator_t *this)
{
	DESTROY_IF(this->current);
	this->inner->destroy(this->inner);
	free(this);
}

/**
 * Implementation of backend_t.create_peer_cfg_enumerator.
 */
static enumerator_t* create_peer_cfg_enumerator(private_medcli_config_t *this,
												identification_t *me,
												identification_t *other)
{
	peer_enumerator_t *e = malloc_thing(peer_enumerator_t);
	
	e->current = NULL;
	e->ike = this->ike;
	e->rekey = this->rekey;
	e->dpd = this->dpd;
	e->public.enumerate = (void*)peer_enumerator_enumerate;
	e->public.destroy = (void*)peer_enumerator_destroy;

	/* filter on IDs: NULL or ANY or matching KEY_ID */
	e->inner = this->db->query(this->db,
			"SELECT Alias, ClientConfig.KeyId, Connection.KeyId, "
			"Connection.LocalSubnet, Connection.RemoteSubnet "
			"FROM ClientConfig JOIN Connection "
			"WHERE Active AND "
			"(? OR ClientConfig.KeyId = ?) AND (? OR Connection.KeyId = ?)", 
			DB_INT, me == NULL || me->get_type(me) == ID_ANY, 
			DB_BLOB, me && me->get_type(me) == ID_KEY_ID ? 
				me->get_encoding(me) : chunk_empty,
			DB_INT, other == NULL || other->get_type(other) == ID_ANY, 
			DB_BLOB, other && other->get_type(other) == ID_KEY_ID ? 
				other->get_encoding(other) : chunk_empty,
			DB_TEXT, DB_BLOB, DB_BLOB, DB_TEXT, DB_TEXT);
	if (!e->inner)
	{
		free(e);
		return NULL;
	}
	return &e->public;
}

/**
 * initiate a peer config
 */
static job_requeue_t initiate_config(peer_cfg_t *peer_cfg)
{
	enumerator_t *enumerator;
	child_cfg_t *child_cfg = NULL;;
	
	enumerator = peer_cfg->create_child_cfg_enumerator(peer_cfg);
	enumerator->enumerate(enumerator, &child_cfg);
	if (child_cfg)
	{
		child_cfg->get_ref(child_cfg);
		peer_cfg->get_ref(peer_cfg);
		enumerator->destroy(enumerator);
		charon->controller->initiate(charon->controller,
									 peer_cfg, child_cfg, NULL, NULL);
	}
	else
	{
		enumerator->destroy(enumerator);
	}
	return JOB_REQUEUE_NONE;
}

/**
 * schedule initation of all "active" connections
 */
static void schedule_autoinit(private_medcli_config_t *this)
{
	enumerator_t *e;
	char *name;
	
	e = this->db->query(this->db, "SELECT Alias FROM Connection WHERE Active",
						DB_TEXT);
	if (e)
	{
		while (e->enumerate(e, &name))
		{
			peer_cfg_t *peer_cfg;
			
			peer_cfg = get_peer_cfg_by_name(this, name);
			if (peer_cfg)
			{
				/* schedule asynchronous initiation job */
				charon->processor->queue_job(charon->processor,
						(job_t*)callback_job_create(
									(callback_job_cb_t)initiate_config,
									peer_cfg, (void*)peer_cfg->destroy, NULL));
			}
		}
		e->destroy(e);
	}
}

/**
 * Implementation of medcli_config_t.destroy.
 */
static void destroy(private_medcli_config_t *this)
{
	this->ike->destroy(this->ike);
	free(this);
}

/**
 * Described in header.
 */
medcli_config_t *medcli_config_create(database_t *db)
{
	private_medcli_config_t *this = malloc_thing(private_medcli_config_t);

	this->public.backend.create_peer_cfg_enumerator = (enumerator_t*(*)(backend_t*, identification_t *me, identification_t *other))create_peer_cfg_enumerator;
	this->public.backend.create_ike_cfg_enumerator = (enumerator_t*(*)(backend_t*, host_t *me, host_t *other))create_ike_cfg_enumerator;
	this->public.backend.get_peer_cfg_by_name = (peer_cfg_t* (*)(backend_t*,char*))get_peer_cfg_by_name;
	this->public.destroy = (void(*)(medcli_config_t*))destroy;
	
	this->db = db;
	this->rekey = lib->settings->get_time(lib->settings, "medcli.rekey", 1200);
	this->dpd = lib->settings->get_time(lib->settings, "medcli.dpd", 300);
	this->ike = ike_cfg_create(FALSE, FALSE, "0.0.0.0", "0.0.0.0");
	this->ike->add_proposal(this->ike, proposal_create_default(PROTO_IKE));
	
	schedule_autoinit(this);
	
	return &this->public;
}

