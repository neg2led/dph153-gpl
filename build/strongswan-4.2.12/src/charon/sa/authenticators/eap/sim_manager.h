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
 */

/**
 * @defgroup sim_manager sim_manager
 * @{ @ingroup eap
 */

#ifndef SIM_MANAGER_H_
#define SIM_MANAGER_H_

#include <utils/identification.h>
#include <utils/enumerator.h>

typedef struct sim_manager_t sim_manager_t;
typedef struct sim_card_t sim_card_t;
typedef struct sim_provider_t sim_provider_t;

/**
 * Interface for a SIM card (used as EAP client).
 */
struct sim_card_t {

	/**
	 * Get the identity of a SIM card.
	 *
	 * The returned identity owned by the sim_card and not destroyed outside.
	 * The SIM card may return ID_ANY if it does not support/use an IMSI.
	 *
	 * @return		identity of type ID_EAP/ID_ANY
	 */
	identification_t* (*get_imsi)(sim_card_t *this);
	
	/**
	 * Calculate SRES/KC from a RAND.
	 *
	 * @param rand	RAND input buffer, fixed size 16 bytes
	 * @param sres	SRES output buffer, fixed size 4 byte
	 * @param kc	KC output buffer, fixed size 8 bytes
	 * @return		TRUE if SRES/KC calculated, FALSE on error
	 */
	bool (*get_triplet)(sim_card_t *this,
						char rand[16], char sres[4], char kc[8]);
};

/**
 * Interface for a triplet provider (used as EAP server).
 */
struct sim_provider_t {
	
	/**
	 * Get a single triplet to authenticate a EAP client.
	 *
	 * @param imsi	client identity of type ID_EAP
	 * @param rand	RAND output buffer, fixed size 16 bytes
	 * @param sres	SRES output buffer, fixed size 4 byte
	 * @param kc	KC output buffer, fixed size 8 bytes
	 * @return		TRUE if triplet received, FALSE otherwise
	 */
	bool (*get_triplet)(sim_provider_t *this, identification_t *imsi,
						char rand[16], char sres[4], char kc[8]);
};

/**
 * The EAP-SIM manager handles multiple SIM cards and providers.
 */
struct sim_manager_t {
	
	/**
	 * Register a SIM card (client) at the manager.
	 *
	 * @param card		sim card to register
	 */
	void (*add_card)(sim_manager_t *this, sim_card_t *card);
	
	/**
	 * Unregister a previously registered card from the manager.
	 *
	 * @param card		sim card to unregister
	 */
	void (*remove_card)(sim_manager_t *this, sim_card_t *card);
	
	/**
	 * Create an enumerator over all registered cards.
	 *
	 * @return			enumerator over sim_card_t's
	 */
	enumerator_t* (*create_card_enumerator)(sim_manager_t *this);
	
	/**
	 * Register a triplet provider (server) at the manager.
	 *
	 * @param card		sim card to register
	 */
	void (*add_provider)(sim_manager_t *this, sim_provider_t *provider);
	
	/**
	 * Unregister a previously registered provider from the manager.
	 *
	 * @param card		sim card to unregister
	 */
	void (*remove_provider)(sim_manager_t *this, sim_provider_t *provider);
	
	/**
	 * Create an enumerator over all registered provider.
	 *
	 * @return			enumerator over sim_provider_t's
	 */
	enumerator_t* (*create_provider_enumerator)(sim_manager_t *this);
	
	/**
	 * Destroy a manager instance.
	 */
	void (*destroy)(sim_manager_t *this);
};

/**
 * Create an SIM manager to handle multiple SIM cards/providers.
 *
 * @return			sim_t object
 */
sim_manager_t *sim_manager_create();

#endif /* SIM_MANAGER_H_ @}*/
