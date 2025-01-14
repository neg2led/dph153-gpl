/*
 * Copyright (C) 2006-2008 Tobias Brunner
 * Copyright (C) 2006-2008 Martin Willi
 * Copyright (C) 2006 Daniel Roethlisberger
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
 * $Id: child_sa.h 4677 2008-11-19 15:31:27Z martin $
 */

/**
 * @defgroup child_sa child_sa
 * @{ @ingroup sa
 */

#ifndef CHILD_SA_H_
#define CHILD_SA_H_

typedef enum child_sa_state_t child_sa_state_t;
typedef struct child_sa_t child_sa_t;

#include <library.h>
#include <crypto/prf_plus.h>
#include <encoding/payloads/proposal_substructure.h>
#include <config/proposal.h>
#include <config/child_cfg.h>

/**
 * States of a CHILD_SA
 */
enum child_sa_state_t {
	
	/**
	 * Just created, uninstalled CHILD_SA
	 */
	CHILD_CREATED,
	
	/**
	 * Installed SPD, but no SAD entries
	 */
	CHILD_ROUTED,
	
	/**
	 * Installing an in-use CHILD_SA
	 */
	CHILD_INSTALLING,
	
	/**
	 * Installed an in-use CHILD_SA
	 */
	CHILD_INSTALLED,
	
	/**
	 * While updating hosts, in update_hosts()
	 */
	CHILD_UPDATING,
	
	/**
	 * CHILD_SA which is rekeying
	 */
	CHILD_REKEYING,
	
	/**
	 * CHILD_SA in progress of delete
	 */
	CHILD_DELETING,
	
	/**
	 * CHILD_SA object gets destroyed
	 */
	CHILD_DESTROYING,
};

/**
 * enum strings for child_sa_state_t.
 */
extern enum_name_t *child_sa_state_names;

/**
 * Represents an IPsec SAs between two hosts.
 * 
 * A child_sa_t contains two SAs. SAs for both
 * directions are managed in one child_sa_t object. Both
 * SAs and the policies have the same reqid.
 * 
 * The procedure for child sa setup is as follows:
 * - A gets SPIs for a all protocols in its proposals via child_sa_t.alloc
 * - A send the proposals with the allocated SPIs to B
 * - B selects a suitable proposal
 * - B allocates an SPI for the selected protocol
 * - B calls child_sa_t.install for both, the allocated and received SPI
 * - B sends the proposal with the allocated SPI to A
 * - A calls child_sa_t.install for both, the allocated and recevied SPI
 * 
 * Once SAs are set up, policies can be added using add_policies.
 */
struct child_sa_t {
	
	/**
	 * Get the name of the config this CHILD_SA uses.
	 *
	 * @return			name
	 */
	char* (*get_name) (child_sa_t *this);
	
	/**
	 * Get the reqid of the CHILD SA.
	 * 
	 * Every CHILD_SA has a reqid. The kernel uses this ID to
	 * identify it.
	 *
	 * @return 			reqid of the CHILD SA
	 */
	u_int32_t (*get_reqid)(child_sa_t *this);
	
	/**
	 * Get the config used to set up this child sa.
	 *
	 * @return			child_cfg
	 */
	child_cfg_t* (*get_config) (child_sa_t *this);
	
	/**
	 * Get the state of the CHILD_SA.
	 *
	 * @return 			CHILD_SA state
	 */	
	child_sa_state_t (*get_state) (child_sa_t *this);
	
	/**
	 * Set the state of the CHILD_SA.
	 *
	 * @param state		state to set on CHILD_SA
	 */	
	void (*set_state) (child_sa_t *this, child_sa_state_t state);
	
	/**
	 * Get the SPI of this CHILD_SA.
	 * 
	 * Set the boolean parameter inbound to TRUE to
	 * get the SPI for which we receive packets, use
	 * FALSE to get those we use for sending packets.
	 *
	 * @param inbound	TRUE to get inbound SPI, FALSE for outbound.
	 * @return 			SPI of the CHILD SA
	 */
	u_int32_t (*get_spi) (child_sa_t *this, bool inbound);
	
	/**
	 * Get the CPI of this CHILD_SA.
	 * 
	 * Set the boolean parameter inbound to TRUE to
	 * get the CPI for which we receive packets, use
	 * FALSE to get those we use for sending packets.
	 *
	 * @param inbound	TRUE to get inbound CPI, FALSE for outbound.
	 * @return 			CPI of the CHILD SA
	 */
	u_int16_t (*get_cpi) (child_sa_t *this, bool inbound);

	/**
	 * Get the protocol which this CHILD_SA uses to protect traffic.
	 *
	 * @return 			AH | ESP
	 */
	protocol_id_t (*get_protocol) (child_sa_t *this);
	
	/**
	 * Set the negotiated protocol to use for this CHILD_SA.
	 *
	 * @param protocol	AH | ESP
	 */
	void (*set_protocol)(child_sa_t *this, protocol_id_t protocol);
	
	/**
	 * Get the IPsec mode of this CHILD_SA.
	 *
	 * @return			TUNNEL | TRANSPORT | BEET
	 */
	ipsec_mode_t (*get_mode)(child_sa_t *this);
	
	/**
	 * Set the negotiated IPsec mode to use.
	 *
	 * @param mode		TUNNEL | TRANPORT | BEET
	 */
	void (*set_mode)(child_sa_t *this, ipsec_mode_t mode);
	
	/**
	 * Get the used IPComp algorithm.
	 *
	 * @return			IPComp compression algorithm.
	 */
	ipcomp_transform_t (*get_ipcomp)(child_sa_t *this);
	
	/**
	 * Set the IPComp algorithm to use.
	 * 
	 * @param ipcomp	the IPComp transform to use
	 */
	void (*set_ipcomp)(child_sa_t *this, ipcomp_transform_t ipcomp);
	
	/**
	 * Get the selected proposal.
	 *
	 * @return			selected proposal
	 */
	proposal_t* (*get_proposal)(child_sa_t *this);
	
	/**
	 * Set the negotiated proposal.
	 *
	 * @param proposal	selected proposal
	 */
	void (*set_proposal)(child_sa_t *this, proposal_t *proposal);	
	
	/**
	 * Check if this CHILD_SA uses UDP encapsulation.
	 *
	 * @return			TRUE if SA encapsulates ESP packets
	 */
	bool (*has_encap)(child_sa_t *this);
	
	/**
	 * Get the lifetime of the CHILD_SA.
	 *
	 * @param hard		TRUE for hard lifetime, FALSE for soft (rekey) lifetime
	 * @return			lifetime in seconds
	 */
	u_int32_t (*get_lifetime)(child_sa_t *this, bool hard);
	
	/**
	 * Get last use time of the CHILD_SA.
	 *
	 * @param inbound	TRUE for inbound traffic, FALSE for outbound
	 * @return			time of last use in seconds
	 */
	u_int32_t (*get_usetime)(child_sa_t *this, bool inbound);
	
	/**
	 * Get the traffic selectors list added for one side.
	 *
	 * @param local		TRUE for own traffic selectors, FALSE for remote
	 * @return			list of traffic selectors
	 */	
	linked_list_t* (*get_traffic_selectors) (child_sa_t *this, bool local);
	
	/**
	 * Create an enumerator over installed policies.
	 *
	 * @return			enumerator over pairs of traffic selectors.
	 */
	enumerator_t* (*create_policy_enumerator)(child_sa_t *this);
	
	/**
	 * Allocate an SPI to include in a proposal.
	 *
	 * @param protocol	protocol to allocate SPI for (ESP|AH)
	 * @param spi		SPI output pointer
	 * @return			SPI, 0 on failure
	 */
	u_int32_t (*alloc_spi)(child_sa_t *this, protocol_id_t protocol);
	
	/**
	 * Allocate a CPI to use for IPComp.
	 *
	 * @return			CPI, 0 on failure
	 */
	u_int16_t (*alloc_cpi)(child_sa_t *this);
	
	/**
	 * Install an IPsec SA for one direction.
	 *
	 * @param encr		encryption key, if any
	 * @param integ		integrity key
	 * @param spi		SPI to use, allocated for inbound
	 * @param cpi		CPI to use, allocated for outbound
	 * @param inbound	TRUE to install an inbound SA, FALSE for outbound
	 * @return			SUCCESS or FAILED
	 */
	status_t (*install)(child_sa_t *this, chunk_t encr, chunk_t integ,
						u_int32_t spi, u_int16_t cpi, bool inbound);
	/**
	 * Install the policies using some traffic selectors.
	 *
	 * Supplied lists of traffic_selector_t's specify the policies
	 * to use for this child sa.
	 *
	 * @param my_ts		traffic selectors for local site
	 * @param other_ts	traffic selectors for remote site
	 * @return			SUCCESS or FAILED
	 */	
	status_t (*add_policies)(child_sa_t *this, linked_list_t *my_ts_list,
							 linked_list_t *other_ts_list);
	/**
	 * Update hosts and ecapulation mode in the kernel SAs and policies.
	 *
	 * @param me		the new local host
	 * @param other		the new remote host
	 * @param vip		virtual IP, if any
	 * @param			TRUE to use UDP encapsulation for NAT traversal
	 * @return			SUCCESS or FAILED
	 */
	status_t (*update)(child_sa_t *this, host_t *me, host_t *other,
					   host_t *vip, bool encap);
	/**
	 * Destroys a child_sa.
	 */
	void (*destroy) (child_sa_t *this);
};

/**
 * Constructor to create a new child_sa_t.
 *
 * @param me				own address
 * @param other				remote address
 * @param my_id				id of own peer
 * @param other_id			id of remote peer
 * @param config			config to use for this CHILD_SA
 * @param reqid				reqid of old CHILD_SA when rekeying, 0 otherwise
 * @param encap				TRUE to enable UDP encapsulation (NAT traversal)
 * @return					child_sa_t object
 */
child_sa_t * child_sa_create(host_t *me, host_t *other, child_cfg_t *config,
							 u_int32_t reqid, bool encap);

#endif /*CHILD_SA_H_ @} */
