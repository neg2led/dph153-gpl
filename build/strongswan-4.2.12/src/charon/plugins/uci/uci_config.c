/*
 * Copyright (C) 2008 Thomas Kallenberg
 * Copyright (C) 2008 Tobias Brunner
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

#include "uci_config.h"
#include "uci_parser.h"

#include <daemon.h>

typedef struct private_uci_config_t private_uci_config_t;

/**
 * Private data of an uci_config_t object
 */
struct private_uci_config_t {

	/**
	 * Public part
	 */
	uci_config_t public;
	
	/**
	 * UCI parser context
	 */
	uci_parser_t *parser;
};

/**
 * enumerator implementation for create_peer_cfg_enumerator
 */
typedef struct {
	/** implements enumerator */
	enumerator_t public;
	/** currently enumerated peer config */
	peer_cfg_t *peer_cfg;
	/** inner uci_parser section enumerator */
	enumerator_t *inner;
} peer_enumerator_t;

/**
 * create a proposal from a string, with fallback to default
 */
static proposal_t *create_proposal(char *string, protocol_id_t proto)
{
	proposal_t *proposal = NULL;
	
	if (string)
	{
		proposal = proposal_create_from_string(proto, string);
	}
	if (!proposal)
	{	/* UCI default is aes/sha1 only */
		if (proto == PROTO_IKE)
		{
			proposal = proposal_create_from_string(proto, 
								"aes128-aes192-aes256-sha1-modp1536-modp2048");
		}
		else
		{
			proposal = proposal_create_from_string(proto, 
								"aes128-aes192-aes256-sha1");
		}
	}
	return proposal;
}

/**
 * create an identity, with fallback to %any
 */
static identification_t *create_id(char *string)
{
	identification_t *id = NULL;
	
	if (string)
	{
		id = identification_create_from_string(string);
	}
	if (!id)
	{
		id = identification_create_from_encoding(ID_ANY, chunk_empty);
	}
	return id;
}

/**
 * create an traffic selector, fallback to dynamic
 */
static traffic_selector_t *create_ts(char *string)
{
	if (string)
	{
		int netbits = 32;
		host_t *net;
		char *pos;
		
		string = strdupa(string);
		pos = strchr(string, '/');
		if (pos)
		{
			*pos++ = '\0';
			netbits = atoi(pos);
		}
		else
		{
			if (strchr(string, ':'))
			{
				netbits = 128;
			}
		}
		net = host_create_from_string(string, 0);
		if (net)
		{
			return traffic_selector_create_from_subnet(net, netbits, 0, 0);
		}
	}
	return traffic_selector_create_dynamic(0, 0, 65535);
}

/**
 * create a rekey time from a string with hours, with fallback
 */
static u_int create_rekey(char *string)
{
	u_int rekey = 0;
	
	if (string)
	{
		rekey = atoi(string);
		if (rekey)
		{
			return rekey * 3600;
		}
	}
	/* every 12 hours */
	return 12 * 3600;
}

/**
 * Implementation of peer_enumerator_t.public.enumerate
 */
static bool peer_enumerator_enumerate(peer_enumerator_t *this, peer_cfg_t **cfg)
{
	char *name, *ike_proposal, *esp_proposal, *ike_rekey, *esp_rekey;
	char *local_id, *local_addr, *local_net;
	char *remote_id, *remote_addr, *remote_net;
	child_cfg_t *child_cfg;
	ike_cfg_t *ike_cfg;
	auth_info_t *auth;
	auth_class_t class;
	
	/* defaults */
	name = "unnamed";
	local_id = NULL;
	remote_id = NULL;
	local_addr = "0.0.0.0";
	remote_addr = "0.0.0.0";
	local_net = NULL;
	remote_net = NULL;
	ike_proposal = NULL;
	esp_proposal = NULL;
	ike_rekey = NULL;
	esp_rekey = NULL;
	
	if (this->inner->enumerate(this->inner, &name, &local_id, &remote_id,
			&local_addr, &remote_addr, &local_net, &remote_net,
			&ike_proposal, &esp_proposal, &ike_rekey, &esp_rekey))
	{
		DESTROY_IF(this->peer_cfg);
		ike_cfg = ike_cfg_create(FALSE, FALSE, local_addr, remote_addr);
		ike_cfg->add_proposal(ike_cfg, create_proposal(ike_proposal, PROTO_IKE));
		this->peer_cfg = peer_cfg_create(
					name, 2, ike_cfg, create_id(local_id), create_id(remote_id),
					CERT_SEND_IF_ASKED, UNIQUE_NO,
					1, create_rekey(ike_rekey), 0,  /* keytries, rekey, reauth */
					1800, 900,						/* jitter, overtime */
					TRUE, 60, 						/* mobike, dpddelay */
					NULL, NULL, 					/* vip, pool */
					FALSE, NULL, NULL); 			/* mediation, med by, peer id */
		auth = this->peer_cfg->get_auth(this->peer_cfg);
		class = AUTH_CLASS_PSK;
		auth->add_item(auth, AUTHN_AUTH_CLASS, &class);
		child_cfg = child_cfg_create(name,
					create_rekey(esp_rekey) + 300, create_rekey(ike_rekey), 300,
					NULL, TRUE,	MODE_TUNNEL, ACTION_NONE, ACTION_NONE, FALSE);
		child_cfg->add_proposal(child_cfg, create_proposal(esp_proposal, PROTO_ESP));
		child_cfg->add_traffic_selector(child_cfg, TRUE, create_ts(local_net));
		child_cfg->add_traffic_selector(child_cfg, FALSE, create_ts(remote_net));
		this->peer_cfg->add_child_cfg(this->peer_cfg, child_cfg);
		*cfg = this->peer_cfg;
		return TRUE;
	}
	return FALSE;
}

/**
 * Implementation of peer_enumerator_t.public.destroy
 */
static void peer_enumerator_destroy(peer_enumerator_t *this)
{
	DESTROY_IF(this->peer_cfg);
	this->inner->destroy(this->inner);
	free(this);
}

/**
 * Implementation of backend_t.create_peer_cfg_enumerator.
 */
static enumerator_t* create_peer_cfg_enumerator(private_uci_config_t *this,
												identification_t *me, 
												identification_t *other)
{
	peer_enumerator_t *e = malloc_thing(peer_enumerator_t);
	
	e->public.enumerate = (void*)peer_enumerator_enumerate;
	e->public.destroy = (void*)peer_enumerator_destroy;
	e->peer_cfg = NULL;
	e->inner = this->parser->create_section_enumerator(this->parser, 
					"local_id", "remote_id", "local_addr", "remote_addr",
					"local_net", "remote_net", "ike_proposal", "esp_proposal",
					"ike_rekey", "esp_rekey", NULL);
	if (!e->inner)
	{
		free(e);
		return NULL;
	}
	return &e->public;
}

/**
 * enumerator implementation for create_ike_cfg_enumerator
 */
typedef struct {
	/** implements enumerator */
	enumerator_t public;
	/** currently enumerated ike config */
	ike_cfg_t *ike_cfg;
	/** inner uci_parser section enumerator */
	enumerator_t *inner;
} ike_enumerator_t;

/**
 * Implementation of peer_enumerator_t.public.enumerate
 */
static bool ike_enumerator_enumerate(ike_enumerator_t *this, ike_cfg_t **cfg)
{
	char *local_addr, *remote_addr, *ike_proposal;
	
	/* defaults */
	local_addr = "0.0.0.0";
	remote_addr = "0.0.0.0";
	ike_proposal = NULL;
	
	if (this->inner->enumerate(this->inner, NULL,
							   &local_addr, &remote_addr, &ike_proposal))
	{
		DESTROY_IF(this->ike_cfg);
		this->ike_cfg = ike_cfg_create(FALSE, FALSE, local_addr, remote_addr);
		this->ike_cfg->add_proposal(this->ike_cfg,
									create_proposal(ike_proposal, PROTO_IKE));

		*cfg = this->ike_cfg;
		return TRUE;
	}
	return FALSE;
}

/**
 * Implementation of ike_enumerator_t.public.destroy
 */
static void ike_enumerator_destroy(ike_enumerator_t *this)
{
	DESTROY_IF(this->ike_cfg);
	this->inner->destroy(this->inner);
	free(this);
}

/**
 * Implementation of backend_t.create_ike_cfg_enumerator.
 */
static enumerator_t* create_ike_cfg_enumerator(private_uci_config_t *this,
											   host_t *me, host_t *other)
{
	ike_enumerator_t *e = malloc_thing(ike_enumerator_t);
	
	e->public.enumerate = (void*)ike_enumerator_enumerate;
	e->public.destroy = (void*)ike_enumerator_destroy;
	e->ike_cfg = NULL;
	e->inner = this->parser->create_section_enumerator(this->parser, 
							"local_addr", "remote_addr", "ike_proposal", NULL);
	if (!e->inner)
	{
		free(e);
		return NULL;
	}
	return &e->public;
}

/**
 * implements backend_t.get_peer_cfg_by_name.
 */
static peer_cfg_t *get_peer_cfg_by_name(private_uci_config_t *this, char *name)
{
	enumerator_t *enumerator;
	peer_cfg_t *current, *found = NULL;
		
	enumerator = create_peer_cfg_enumerator(this, NULL, NULL);
	if (enumerator)
	{
		while (enumerator->enumerate(enumerator, &current))
		{
			if (streq(name, current->get_name(current)))
			{
				found = current->get_ref(current);
				break;
			}
		}
		enumerator->destroy(enumerator);
	}
	return found;
}

/**
 * Implementation of uci_config_t.destroy.
 */
static void destroy(private_uci_config_t *this)
{
	free(this);
}

/**
 * Described in header.
 */
uci_config_t *uci_config_create(uci_parser_t *parser)
{
	private_uci_config_t *this = malloc_thing(private_uci_config_t);

	this->public.backend.create_peer_cfg_enumerator = (enumerator_t*(*)(backend_t*, identification_t *me, identification_t *other))create_peer_cfg_enumerator;
	this->public.backend.create_ike_cfg_enumerator = (enumerator_t*(*)(backend_t*, host_t *me, host_t *other))create_ike_cfg_enumerator;
	this->public.backend.get_peer_cfg_by_name = (peer_cfg_t* (*)(backend_t*,char*))get_peer_cfg_by_name;
	this->public.destroy = (void(*)(uci_config_t*))destroy;
	this->parser = parser;

	return &this->public;
}

