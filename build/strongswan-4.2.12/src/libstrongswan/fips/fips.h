/*
 * Copyright (C) 2007 Bruno Krieg, Daniel Wydler
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
 * $Id: fips.h 3877 2008-04-26 09:40:22Z andreas $
 */
 
/**
 * @defgroup fips1 fips
 * @{ @ingroup fips
 */

#ifndef FIPS_H_
#define FIPS_H_

#include <library.h>

/**
 * compute HMAC signature over RODATA and TEXT sections of libstrongswan
 *
 * @param key		key used for HMAC signature in ASCII string format
 * @param signature	HMAC signature in HEX string format
 * @return 			TRUE if HMAC signature computation was successful
 */
bool fips_compute_hmac_signature(const char *key, char *signature);

/**
 * verify HMAC signature over RODATA and TEXT sections of libstrongswan
 *
 * @param key		key used for HMAC signature in ASCII string format
 * @param signature	signature value from fips_signature.h in HEX string format
 * @return			TRUE if signatures agree
 */
bool fips_verify_hmac_signature(const char *key, const char *signature);

#endif /*FIPS_H_ @} */
