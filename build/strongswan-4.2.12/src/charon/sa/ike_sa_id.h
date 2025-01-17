/*
 * Copyright (C) 2005-2006 Martin Willi
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
 * $Id: ike_sa_id.h 3589 2008-03-13 14:14:44Z martin $
 */

/**
 * @defgroup ike_sa_id ike_sa_id
 * @{ @ingroup sa
 */

#ifndef IKE_SA_ID_H_
#define IKE_SA_ID_H_

typedef struct ike_sa_id_t ike_sa_id_t;

#include <library.h>

/**
 * An object of type ike_sa_id_t is used to identify an IKE_SA.
 *
 * An IKE_SA is identified by its initiator and responder spi's.
 * Additionaly it contains the role of the actual running IKEv2-Daemon
 * for the specific IKE_SA (original initiator or responder).
 */
struct ike_sa_id_t {

	/**
	 * Set the SPI of the responder.
	 *
	 * This function is called when a request or reply of a IKE_SA_INIT is received.
	 *
	 * @param responder_spi 	SPI of responder to set
	 */
	void (*set_responder_spi) (ike_sa_id_t *this, u_int64_t responder_spi);

	/**
	 * Set the SPI of the initiator.
	 *
	 * @param initiator_spi 	SPI to set
	 */
	void (*set_initiator_spi) (ike_sa_id_t *this, u_int64_t initiator_spi);

	/**
	 * Get the initiator SPI.
	 *
	 * @return 					SPI of the initiator
	 */
	u_int64_t (*get_initiator_spi) (ike_sa_id_t *this);

	/**
	 * Get the responder SPI.
	 *
	 * @return 					SPI of the responder
	 */
	u_int64_t (*get_responder_spi) (ike_sa_id_t *this);

	/**
	 * Check if two ike_sa_id_t objects are equal.
	 * 
	 * Two ike_sa_id_t objects are equal if both SPI values and the role matches.
	 *
 	 * @param other 			ike_sa_id_t object to check if equal
 	 * @return 					TRUE if given ike_sa_id_t are equal, FALSE otherwise
	 */
	bool (*equals) (ike_sa_id_t *this, ike_sa_id_t *other);

	/**
	 * Replace all values of a given ike_sa_id_t object with values.
	 * from another ike_sa_id_t object.
	 * 
	 * After calling this function, both objects are equal.
	 *
 	 * @param other 			ike_sa_id_t object from which values will be taken
	 */
	void (*replace_values) (ike_sa_id_t *this, ike_sa_id_t *other);

	/**
	 * Get the initiator flag.
	 *
	 * @return 					TRUE if we are the original initator
	 */
	bool (*is_initiator) (ike_sa_id_t *this);

	/**
	 * Switche the original initiator flag.
	 * 
	 * @return 					TRUE if we are the original initator after switch, FALSE otherwise
	 */
	bool (*switch_initiator) (ike_sa_id_t *this);

	/**
	 * Clones a given ike_sa_id_t object.
	 *
	 * @return 					cloned ike_sa_id_t object
	 */
	ike_sa_id_t *(*clone) (ike_sa_id_t *this);

	/**
	 * Destroys an ike_sa_id_t object.
	 */
	void (*destroy) (ike_sa_id_t *this);
};

/**
 * Creates an ike_sa_id_t object with specific SPI's and defined role.
 *
 * @param initiator_spi			initiators SPI
 * @param responder_spi			responders SPI
 * @param is_initiaor			TRUE if we are the original initiator
 * @return						ike_sa_id_t object
 */
ike_sa_id_t * ike_sa_id_create(u_int64_t initiator_spi, u_int64_t responder_spi,
							   bool is_initiaor);

#endif /*IKE_SA_ID_H_ @} */
