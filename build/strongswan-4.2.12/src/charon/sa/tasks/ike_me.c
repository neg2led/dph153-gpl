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
 * $Id: ike_me.c 4640 2008-11-12 16:07:17Z martin $
 */
 
#include "ike_me.h"

#include <string.h>

#include <daemon.h>
#include <config/peer_cfg.h>
#include <encoding/payloads/id_payload.h>
#include <encoding/payloads/notify_payload.h>
#include <encoding/payloads/endpoint_notify.h>
#include <processing/jobs/mediation_job.h>

#define ME_CONNECTID_LEN 4
#define ME_CONNECTKEY_LEN 16

typedef struct private_ike_me_t private_ike_me_t;

/**
 * Private members of a ike_me_t task.
 */
struct private_ike_me_t {
	
	/**
	 * Public methods and task_t interface.
	 */
	ike_me_t public;
	
	/**
	 * Assigned IKE_SA.
	 */
	ike_sa_t *ike_sa;
	
	/**
	 * Are we the initiator?
	 */
	bool initiator;
	
	/**
	 * Is this a mediation connection?
	 */
	bool mediation;
	
	/**
	 * Is this the response from another peer?
	 */
	bool response;
	
	/**
	 * Gathered endpoints
	 */
	linked_list_t *local_endpoints;
	
	/**
	 * Parsed endpoints
	 */
	linked_list_t *remote_endpoints;
	
	/**
	 * Did the peer request a callback?
	 */
	bool callback;
	
	/**
	 * Did the connect fail?
	 */
	bool failed;
	
	/**
	 * Was there anything wrong with the payloads?
	 */
	bool invalid_syntax;
	
	/**
	 * The requested peer
	 */
	identification_t *peer_id;	
	/**
	 * Received ID used for connectivity checks
	 */
	chunk_t connect_id;
	
	/**
	 * Received key used for connectivity checks
	 */
	chunk_t connect_key;
	
	/**
	 * Peer config of the mediated connection
	 */
	peer_cfg_t *mediated_cfg;

};

/**
 * Adds a list of endpoints as notifies to a given message
 */
static void add_endpoints_to_message(message_t *message, linked_list_t *endpoints)
{
	iterator_t *iterator;
	endpoint_notify_t *endpoint;
	
	iterator = endpoints->create_iterator(endpoints, TRUE);
	while (iterator->iterate(iterator, (void**)&endpoint))
	{
		message->add_payload(message, (payload_t*)endpoint->build_notify(endpoint));
	}
	iterator->destroy(iterator);
}

/**
 * Gathers endpoints and adds them to the current message
 */
static void gather_and_add_endpoints(private_ike_me_t *this, message_t *message)
{
	enumerator_t *enumerator;
	host_t *addr, *host;
	u_int16_t port;
	
	/* get the port that is used to communicate with the ms */
	host = this->ike_sa->get_my_host(this->ike_sa);
	port = host->get_port(host);
	
	enumerator = charon->kernel_interface->create_address_enumerator(
										charon->kernel_interface, FALSE, FALSE);
	while (enumerator->enumerate(enumerator, (void**)&addr))
	{
		host = addr->clone(addr);
		host->set_port(host, port);
		
		this->local_endpoints->insert_last(this->local_endpoints,
				endpoint_notify_create_from_host(HOST, host, NULL));
		
		host->destroy(host);
	}
	enumerator->destroy(enumerator);
	
	host = this->ike_sa->get_server_reflexive_host(this->ike_sa);
	if (host)
	{
		this->local_endpoints->insert_last(this->local_endpoints,
				endpoint_notify_create_from_host(SERVER_REFLEXIVE, host,
						this->ike_sa->get_my_host(this->ike_sa)));
	}
	
	add_endpoints_to_message(message, this->local_endpoints);
}

/**
 * read notifys from message and evaluate them
 */
static void process_payloads(private_ike_me_t *this, message_t *message)
{
	iterator_t *iterator;
	payload_t *payload;

	iterator = message->get_payload_iterator(message);
	while (iterator->iterate(iterator, (void**)&payload))
	{
		if (payload->get_type(payload) != NOTIFY)
		{
			continue;
		}
		
		notify_payload_t *notify = (notify_payload_t*)payload;
		
		switch (notify->get_notify_type(notify))
		{
			case ME_CONNECT_FAILED:
			{
				DBG2(DBG_IKE, "received ME_CONNECT_FAILED notify");
				this->failed = TRUE;
				break;
			}
			case ME_MEDIATION:
			{
				DBG2(DBG_IKE, "received ME_MEDIATION notify");
				this->mediation = TRUE;
				break;
			}
			case ME_ENDPOINT:
			{
				endpoint_notify_t *endpoint = endpoint_notify_create_from_payload(notify);
				if (!endpoint)
				{
					DBG1(DBG_IKE, "received invalid ME_ENDPOINT notify");
					break;
				}
				DBG1(DBG_IKE, "received %N ME_ENDPOINT %#H", me_endpoint_type_names,
					endpoint->get_type(endpoint), endpoint->get_host(endpoint));
				
				this->remote_endpoints->insert_last(this->remote_endpoints, endpoint);
				break;
			}
			case ME_CALLBACK:
			{
				DBG2(DBG_IKE, "received ME_CALLBACK notify");
				this->callback = TRUE;
				break;
			}
			case ME_CONNECTID:
			{
				chunk_free(&this->connect_id);
				this->connect_id = chunk_clone(notify->get_notification_data(notify));
				DBG2(DBG_IKE, "received ME_CONNECTID %#B", &this->connect_id);
				break;
			}
			case ME_CONNECTKEY:
			{
				chunk_free(&this->connect_key);
				this->connect_key = chunk_clone(notify->get_notification_data(notify));
				DBG4(DBG_IKE, "received ME_CONNECTKEY %#B", &this->connect_key);
				break;
			}
			case ME_RESPONSE:
			{
				DBG2(DBG_IKE, "received ME_RESPONSE notify");
				this->response = TRUE;
				break;
			}
			default:
				break;
		}
	}
	iterator->destroy(iterator);
}

/**
 * Implementation of task_t.build for initiator
 */
static status_t build_i(private_ike_me_t *this, message_t *message)
{
	switch(message->get_exchange_type(message))
	{
		case IKE_SA_INIT:
		{
			peer_cfg_t *peer_cfg = this->ike_sa->get_peer_cfg(this->ike_sa);
			if (peer_cfg->is_mediation(peer_cfg))
			{
				DBG2(DBG_IKE, "adding ME_MEDIATION");
				message->add_notify(message, FALSE, ME_MEDIATION, chunk_empty);
			}
			else
			{
				return SUCCESS;
			}
			break;
		}
		case IKE_AUTH:
		{
			if (this->ike_sa->has_condition(this->ike_sa, COND_NAT_HERE))
			{
				endpoint_notify_t *endpoint = endpoint_notify_create_from_host(SERVER_REFLEXIVE, NULL, NULL);
				message->add_payload(message, (payload_t*)endpoint->build_notify(endpoint));
				endpoint->destroy(endpoint);
			}
			break;
		}
		case ME_CONNECT:
		{
			id_payload_t *id_payload;
			rng_t *rng;
			
			id_payload = id_payload_create_from_identification(ID_PEER, this->peer_id);
			message->add_payload(message, (payload_t*)id_payload);
			
			rng = lib->crypto->create_rng(lib->crypto, RNG_STRONG);
			if (!rng)
			{
				DBG1(DBG_IKE, "unable to generate connect ID for ME_CONNECT");	
				return FAILED;
			}
			if (!this->response)
			{
				/* only the initiator creates a connect ID. the responder returns
				 * the connect ID that it received from the initiator */
				rng->allocate_bytes(rng, ME_CONNECTID_LEN, &this->connect_id);
			}
			rng->allocate_bytes(rng, ME_CONNECTKEY_LEN, &this->connect_key);
			rng->destroy(rng);
			
			message->add_notify(message, FALSE, ME_CONNECTID, this->connect_id);
			message->add_notify(message, FALSE, ME_CONNECTKEY, this->connect_key);
			
			if (this->response)
			{
				message->add_notify(message, FALSE, ME_RESPONSE, chunk_empty);
			}
			else
			{
				/* FIXME: should we make that configurable? */
				message->add_notify(message, FALSE, ME_CALLBACK, chunk_empty);
			}
			
			gather_and_add_endpoints(this, message);
			
			break;
		}
		default:
			break;
	}
	return NEED_MORE;
}

/**
 * Implementation of task_t.process for responder
 */
static status_t process_r(private_ike_me_t *this, message_t *message)
{
	switch(message->get_exchange_type(message))
	{
		case ME_CONNECT:
		{
			id_payload_t *id_payload;
			id_payload = (id_payload_t*)message->get_payload(message, ID_PEER);
			if (!id_payload)
			{
				DBG1(DBG_IKE, "received ME_CONNECT without ID_PEER payload, aborting");
				break;
			}
			this->peer_id = id_payload->get_identification(id_payload);
			
			process_payloads(this, message);
			
			if (this->callback)
			{
				DBG1(DBG_IKE, "received ME_CALLBACK for '%D'", this->peer_id);
				break;
			}			
			
			if (!this->connect_id.ptr)
			{
				DBG1(DBG_IKE, "received ME_CONNECT without ME_CONNECTID notify, aborting");
				this->invalid_syntax = TRUE;
				break;
			}
			
			if (!this->connect_key.ptr)
			{
				DBG1(DBG_IKE, "received ME_CONNECT without ME_CONNECTKEY notify, aborting");
				this->invalid_syntax = TRUE;
				break;
			}
			
			if (!this->remote_endpoints->get_count(this->remote_endpoints))
			{
				DBG1(DBG_IKE, "received ME_CONNECT without any ME_ENDPOINT payloads, aborting");
				this->invalid_syntax = TRUE;
				break;
			}
			
			DBG1(DBG_IKE, "received ME_CONNECT");
			break;
		}
		default:
			break;
	}
	return NEED_MORE;
}

/**
 * Implementation of task_t.build for responder
 */
static status_t build_r(private_ike_me_t *this, message_t *message)
{
	switch(message->get_exchange_type(message))
	{
		case ME_CONNECT:
		{
			if (this->invalid_syntax)
			{
				message->add_notify(message, TRUE, INVALID_SYNTAX, chunk_empty);
				break;
			}
			
			if (this->callback)
			{
				charon->connect_manager->check_and_initiate(charon->connect_manager,
						this->ike_sa->get_id(this->ike_sa),
						this->ike_sa->get_my_id(this->ike_sa), this->peer_id);
				return SUCCESS;
			}
			
			if (this->response)
			{
				/* FIXME: handle result of set_responder_data
				 * as initiator, upon receiving a response from another peer,
				 * update the checklist and start sending checks */
				charon->connect_manager->set_responder_data(charon->connect_manager,
						this->connect_id, this->connect_key, this->remote_endpoints);
			}
			else
			{
				/* FIXME: handle result of set_initiator_data
				 * as responder, create a checklist with the initiator's data */
				charon->connect_manager->set_initiator_data(charon->connect_manager,
						this->peer_id, this->ike_sa->get_my_id(this->ike_sa),
						this->connect_id, this->connect_key, this->remote_endpoints,
						FALSE);
				if (this->ike_sa->respond(this->ike_sa, this->peer_id,
						this->connect_id) != SUCCESS)
				{
					return FAILED;
				}
			}
			break;
		}
		default:
			break;
	}
	return SUCCESS;
}

/**
 * Implementation of task_t.process for initiator
 */
static status_t process_i(private_ike_me_t *this, message_t *message)
{
	switch(message->get_exchange_type(message))
	{
		case IKE_SA_INIT:
		{
			process_payloads(this, message);
		
			if (!this->mediation)
			{
				DBG1(DBG_IKE, "server did not return a ME_MEDIATION, aborting");
				return FAILED;
			}
	
			return NEED_MORE;
		}
		case IKE_AUTH:
		{
			process_payloads(this, message);
			/* FIXME: we should update the server reflexive endpoint somehow,
			 * if mobike notices a change */
			endpoint_notify_t *reflexive;
			if (this->remote_endpoints->get_first(this->remote_endpoints, 
											(void**)&reflexive) == SUCCESS &&
				reflexive->get_type(reflexive) == SERVER_REFLEXIVE)
			{	/* FIXME: should we accept this endpoint even if we did not send 
				 * a request? */
				host_t *endpoint = reflexive->get_host(reflexive);
				
				this->ike_sa->set_server_reflexive_host(this->ike_sa, endpoint->clone(endpoint));
			}
			/* FIXME: what if it failed? e.g. AUTH failure */
			DBG1(DBG_IKE, "established mediation connection successfully");
			
			break;
		}
		case ME_CONNECT:
		{
			process_payloads(this, message);
			
			if (this->failed)
			{
				DBG1(DBG_IKE, "peer '%D' is not online", this->peer_id);
				/* FIXME: notify the mediated connection (job?) */
			}
			else
			{
				if (this->response)
				{
					/* FIXME: handle result of set_responder_data.
					 * as responder, we update the checklist and start sending checks */
					charon->connect_manager->set_responder_data(charon->connect_manager,
							this->connect_id, this->connect_key, this->local_endpoints);
				}
				else
				{
					/* FIXME: handle result of set_initiator_data
					 * as initiator, we create a checklist and set the initiator's data */
					charon->connect_manager->set_initiator_data(charon->connect_manager,
						this->ike_sa->get_my_id(this->ike_sa), this->peer_id,
						this->connect_id, this->connect_key, this->local_endpoints,
						TRUE);
					/* FIXME: also start a timer for the whole transaction (maybe
					 * within the connect_manager?) */
				}
			}
			break;
		}
		default:
			break;
	}
	return SUCCESS;
}

/**
 * Implementation of task_t.build for initiator (mediation server)
 */
static status_t build_i_ms(private_ike_me_t *this, message_t *message)
{
	switch(message->get_exchange_type(message))
	{
		case ME_CONNECT:
		{
			id_payload_t *id_payload = id_payload_create_from_identification(ID_PEER, this->peer_id);
			message->add_payload(message, (payload_t*)id_payload);
			
			if (this->callback)
			{
				message->add_notify(message, FALSE, ME_CALLBACK, chunk_empty);
			}
			else
			{
				if (this->response)
				{
					message->add_notify(message, FALSE, ME_RESPONSE, chunk_empty);
				}	
				message->add_notify(message, FALSE, ME_CONNECTID, this->connect_id);
				message->add_notify(message, FALSE, ME_CONNECTKEY, this->connect_key);
				
				add_endpoints_to_message(message, this->remote_endpoints);
			}
			break;
		}
		default:
			break;
	}
	
	return NEED_MORE;
}

/**
 * Implementation of task_t.process for responder (mediation server)
 */
static status_t process_r_ms(private_ike_me_t *this, message_t *message)
{
	switch(message->get_exchange_type(message))
	{
		case IKE_SA_INIT:
		{
			/* FIXME: we should check for SA* and TS* payloads
			 * if any are there send NO_ADDITIONAL_SAS back and delete this SA */
			process_payloads(this, message);
			return this->mediation ? NEED_MORE : SUCCESS;
		}
		case IKE_AUTH:
		{
			/* FIXME: we should check whether the current peer_config is configured
			 * as mediation connection */
			process_payloads(this, message);
			break;
		}
		case CREATE_CHILD_SA:
		{
			/* FIXME: if this is not to rekey the IKE SA we have to return a
			 * NO_ADDITIONAL_SAS and then delete the SA */
			break;
		}
		case ME_CONNECT:
		{
			id_payload_t *id_payload;
			id_payload = (id_payload_t*)message->get_payload(message, ID_PEER);
			if (!id_payload)
			{
				DBG1(DBG_IKE, "received ME_CONNECT without ID_PEER payload, aborting");
				this->invalid_syntax = TRUE;
				break;
			}
			
			this->peer_id = id_payload->get_identification(id_payload);
			
			process_payloads(this, message);
			
			if (!this->connect_id.ptr)
			{
				DBG1(DBG_IKE, "received ME_CONNECT without ME_CONNECTID notify, aborting");
				this->invalid_syntax = TRUE;
				break;
			}
			
			if (!this->connect_key.ptr)
			{
				DBG1(DBG_IKE, "received ME_CONNECT without ME_CONNECTKEY notify, aborting");
				this->invalid_syntax = TRUE;
				break;
			}
			
			if (!this->remote_endpoints->get_count(this->remote_endpoints))
			{
				DBG1(DBG_IKE, "received ME_CONNECT without any ME_ENDPOINT payloads, aborting");
				this->invalid_syntax = TRUE;
				break;
			}
			break;
		}
		default:
			break;
	}
	
	return NEED_MORE;
}

/**
 * Implementation of task_t.build for responder (mediation server)
 */
static status_t build_r_ms(private_ike_me_t *this, message_t *message)
{
	switch(message->get_exchange_type(message))
	{
		case IKE_SA_INIT:
		{
			message->add_notify(message, FALSE, ME_MEDIATION, chunk_empty);
			return NEED_MORE;
		}
		case IKE_AUTH:
		{
			endpoint_notify_t *endpoint;
			if (this->remote_endpoints->get_first(this->remote_endpoints, (void**)&endpoint) == SUCCESS &&
					endpoint->get_type(endpoint) == SERVER_REFLEXIVE)
			{
				host_t *host = this->ike_sa->get_other_host(this->ike_sa);
				
				DBG2(DBG_IKE, "received request for a server reflexive endpoint "
						"sending: %#H", host);
				
				endpoint = endpoint_notify_create_from_host(SERVER_REFLEXIVE, host, NULL);								
				message->add_payload(message, (payload_t*)endpoint->build_notify(endpoint));
				endpoint->destroy(endpoint);
			}
			
			/* FIXME: we actually must delete any existing IKE_SAs with the same remote id */
			this->ike_sa->act_as_mediation_server(this->ike_sa);
			
			DBG1(DBG_IKE, "established mediation connection successfully");
			
			break;
		}
		case ME_CONNECT:
		{	
			if (this->invalid_syntax)
			{
				message->add_notify(message, TRUE, INVALID_SYNTAX, chunk_empty);
				break;
			}
			
			ike_sa_id_t *peer_sa;
			if (this->callback)
			{
				peer_sa = charon->mediation_manager->check_and_register(charon->mediation_manager,
						this->peer_id, this->ike_sa->get_other_id(this->ike_sa));
			}
			else
			{
				peer_sa = charon->mediation_manager->check(charon->mediation_manager,
						this->peer_id);
			}
			
			if (!peer_sa)
			{
				/* the peer is not online */
				message->add_notify(message, TRUE, ME_CONNECT_FAILED, chunk_empty);
				break;
			}
			
			job_t *job = (job_t*)mediation_job_create(this->peer_id,
					this->ike_sa->get_other_id(this->ike_sa), this->connect_id,
					this->connect_key, this->remote_endpoints, this->response);
			charon->processor->queue_job(charon->processor, job);
			
			break;
		}
		default:
			break;
	}
	return SUCCESS;
}

/**
 * Implementation of task_t.process for initiator (mediation server)
 */
static status_t process_i_ms(private_ike_me_t *this, message_t *message)
{
	/* FIXME: theoretically we should be prepared to receive a ME_CONNECT_FAILED
	 * here if the responding peer is not able to proceed. in this case we shall
	 * notify the initiating peer with a ME_CONNECT request containing only a
	 * ME_CONNECT_FAILED */
	return SUCCESS;
}

/**
 * Implementation of ike_me.connect
 */
static void me_connect(private_ike_me_t *this, identification_t *peer_id)
{
	this->peer_id = peer_id->clone(peer_id);
}

/**
 * Implementation of ike_me.respond
 */
static void me_respond(private_ike_me_t *this, identification_t *peer_id, 
		chunk_t connect_id)
{
	this->peer_id = peer_id->clone(peer_id);
	this->connect_id = chunk_clone(connect_id);
	this->response = TRUE;
}

/**
 * Implementation of ike_me.callback
 */
static void me_callback(private_ike_me_t *this, identification_t *peer_id)
{
	this->peer_id = peer_id->clone(peer_id);
	this->callback = TRUE;
}

/**
 * Implementation of ike_me.relay
 */
static void relay(private_ike_me_t *this, identification_t *requester, chunk_t connect_id,
		chunk_t connect_key, linked_list_t *endpoints, bool response)
{
	this->peer_id = requester->clone(requester);
	this->connect_id = chunk_clone(connect_id);
	this->connect_key = chunk_clone(connect_key);
	
	this->remote_endpoints->destroy_offset(this->remote_endpoints, offsetof(endpoint_notify_t, destroy));
	this->remote_endpoints = endpoints->clone_offset(endpoints, offsetof(endpoint_notify_t, clone));
	
	this->response = response;
}

/**
 * Implementation of task_t.get_type
 */
static task_type_t get_type(private_ike_me_t *this)
{
	return IKE_ME;
}

/**
 * Implementation of task_t.migrate
 */
static void migrate(private_ike_me_t *this, ike_sa_t *ike_sa)
{
	this->ike_sa = ike_sa;
}

/**
 * Implementation of task_t.destroy
 */
static void destroy(private_ike_me_t *this)
{
	DESTROY_IF(this->peer_id);
	
	chunk_free(&this->connect_id);
	chunk_free(&this->connect_key);
	
	this->local_endpoints->destroy_offset(this->local_endpoints, offsetof(endpoint_notify_t, destroy));
	this->remote_endpoints->destroy_offset(this->remote_endpoints, offsetof(endpoint_notify_t, destroy));
	
	DESTROY_IF(this->mediated_cfg);
	free(this);
}

/*
 * Described in header.
 */
ike_me_t *ike_me_create(ike_sa_t *ike_sa, bool initiator)
{
	private_ike_me_t *this = malloc_thing(private_ike_me_t);

	this->public.task.get_type = (task_type_t(*)(task_t*))get_type;
	this->public.task.migrate = (void(*)(task_t*,ike_sa_t*))migrate;
	this->public.task.destroy = (void(*)(task_t*))destroy;
	
	if (ike_sa->has_condition(ike_sa, COND_ORIGINAL_INITIATOR))
	{
		if (initiator)
		{
			this->public.task.build = (status_t(*)(task_t*,message_t*))build_i;
			this->public.task.process = (status_t(*)(task_t*,message_t*))process_i;
		}
		else
		{
			this->public.task.build = (status_t(*)(task_t*,message_t*))build_r;
			this->public.task.process = (status_t(*)(task_t*,message_t*))process_r;
		}
	}
	else
	{
		/* mediation server */
		if (initiator)
		{
			this->public.task.build = (status_t(*)(task_t*,message_t*))build_i_ms;
			this->public.task.process = (status_t(*)(task_t*,message_t*))process_i_ms;
		}
		else
		{
			this->public.task.build = (status_t(*)(task_t*,message_t*))build_r_ms;
			this->public.task.process = (status_t(*)(task_t*,message_t*))process_r_ms;
		}
	}
	
	this->public.connect = (void(*)(ike_me_t*,identification_t*))me_connect;
	this->public.respond = (void(*)(ike_me_t*,identification_t*,chunk_t))me_respond;
	this->public.callback = (void(*)(ike_me_t*,identification_t*))me_callback;
	this->public.relay = (void(*)(ike_me_t*,identification_t*,chunk_t,chunk_t,linked_list_t*,bool))relay;
	
	this->ike_sa = ike_sa;
	this->initiator = initiator;
	
	this->peer_id = NULL;
	this->connect_id = chunk_empty;
	this->connect_key = chunk_empty;
	this->local_endpoints = linked_list_create();
	this->remote_endpoints = linked_list_create();
	this->mediation = FALSE;
	this->response = FALSE;
	this->callback = FALSE;
	this->failed = FALSE;
	this->invalid_syntax = FALSE;
	
	this->mediated_cfg = NULL;
	
	return &this->public;
}
