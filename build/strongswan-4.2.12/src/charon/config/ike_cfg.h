/*
 * Copyright (C) 2005-2007 Martin Willi
 * Copyright (C) 2005 Jan Hutter
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
 * $Id: ike_cfg.h 4044 2008-06-06 15:05:54Z martin $
 */

/**
 * @defgroup ike_cfg ike_cfg
 * @{ @ingroup config
 */

#ifndef IKE_CFG_H_
#define IKE_CFG_H_

typedef struct ike_cfg_t ike_cfg_t;

#include <library.h>
#include <utils/host.h>
#include <utils/linked_list.h>
#include <utils/identification.h>
#include <config/proposal.h>
#include <crypto/diffie_hellman.h>

/**
 * An ike_cfg_t defines the rules to set up an IKE_SA.
 *
 * @see peer_cfg_t to get an overview over the configurations.
 */
struct ike_cfg_t {
	
	/**
	 * Get own address.
	 * 
	 * @return		string of address/DNS name
	 */
	char* (*get_my_addr) (ike_cfg_t *this);

	/**
	 * Get peers address.
	 * 
	 * @return		string of address/DNS name
	 */
	char* (*get_other_addr) (ike_cfg_t *this);
	
	/**
	 * Adds a proposal to the list.
	 * 
	 * The first added proposal has the highest priority, the last
	 * added the lowest.
	 * 
	 * @param proposal	proposal to add
	 */
	void (*add_proposal) (ike_cfg_t *this, proposal_t *proposal);
	
	/**
	 * Returns a list of all supported proposals.
	 * 
	 * Returned list and its proposals must be destroyed after use.
	 * 
	 * @return 			list containing all the proposals
	 */
	linked_list_t* (*get_proposals) (ike_cfg_t *this);
	
	/**
	 * Select a proposed from suggested proposals.
	 * 
	 * Returned proposal must be destroyed after use.
	 * 
	 * @param proposals	list of proposals to select from
	 * @return			selected proposal, or NULL if none matches.
	 */
	proposal_t *(*select_proposal) (ike_cfg_t *this, linked_list_t *proposals);
	
	/**
	 * Should we send a certificate request in IKE_SA_INIT?
	 *
	 * @return			certificate request sending policy
	 */
	bool (*send_certreq) (ike_cfg_t *this);
	
	/**
	 * Enforce UDP encapsulation by faking NATD notifies?
	 * 
	 * @return			TRUE to enfoce UDP encapsulation
	 */
	bool (*force_encap) (ike_cfg_t *this);
	
	/**
	 * Get the DH group to use for IKE_SA setup.
	 * 
	 * @return			dh group to use for initialization
	 */
	diffie_hellman_group_t (*get_dh_group)(ike_cfg_t *this);
	
	/**
	 * Check if two IKE configs are equal.
	 *
	 * @param other		other to check for equality
	 * @return			TRUE if other equal to this
	 */
	bool (*equals)(ike_cfg_t *this, ike_cfg_t *other);
	
	/**
	 * Increase reference count.
	 *
	 * @return			reference to this
	 */
	ike_cfg_t* (*get_ref) (ike_cfg_t *this);
	
	/**
	 * Destroys a ike_cfg_t object.
	 * 
	 * Decrements the internal reference counter and
	 * destroys the ike_cfg when it reaches zero.
	 */
	void (*destroy) (ike_cfg_t *this);
};

/**
 * Creates a ike_cfg_t object.
 *
 * Supplied hosts become owned by ike_cfg, the name gets cloned.
 *
 * @param name			ike_cfg identifier
 * @param certreq		TRUE to send a certificate request
 * @param force_encap	enforce UDP encapsulation by faking NATD notify
 * @param me			address/DNS name of local peer
 * @param other			address/DNS name of remote peer
 * @return 				ike_cfg_t object.
 */
ike_cfg_t *ike_cfg_create(bool certreq, bool force_encap, 
						  char *me, char *other);

#endif /* IKE_CFG_H_ @} */
