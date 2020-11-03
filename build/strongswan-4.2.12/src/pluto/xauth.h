/* Interface definition of the XAUTH server and|or client module
 * Copyright (C) 2006 Andreas Steffen
 * Hochschule fuer Technik Rapperswil, Switzerland
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
 * RCSID $Id: xauth.h 3738 2008-04-02 19:04:45Z andreas $
 */

#ifndef _XAUTH_H
#define _XAUTH_H

#include <freeswan.h>
#include "defs.h"

/* XAUTH credentials */

struct chunk_t;

typedef struct {
    char *conn_name;
    char id[BUF_LEN];
    char ip_address[ADDRTOT_BUF];
} xauth_peer_t;

typedef struct {
    chunk_t user_name;
    chunk_t user_password;
} xauth_t;

typedef struct {
    void *handle;
    bool (*get_secret) (xauth_t *xauth_secret);
    bool (*verify_secret) (const xauth_peer_t *peer, const xauth_t *xauth_secret);
} xauth_module_t;

extern xauth_module_t xauth_module;

extern void xauth_init(void);
extern void xauth_finalize(void);

#endif /* _XAUTH_H */
