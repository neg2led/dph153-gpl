/*
 * Copyright (C) 2001-2008 Andreas Steffen
 *
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
 * $Id: pem.h 4011 2008-05-23 19:18:08Z andreas $
 */

#ifndef PEM_H_
#define PEM_H_

#include <stdio.h>

#include <library.h>

bool pem_to_bin(chunk_t *blob, chunk_t *passphrase, bool *pgp);

bool pem_asn1_load_file(char *filename, chunk_t *passphrase,
						chunk_t *blob, bool *pgp);

#endif /*PEM_H_ @} */
