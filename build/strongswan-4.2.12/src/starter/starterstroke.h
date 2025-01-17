/* Stroke for charon is the counterpart to whack from pluto
 * Copyright (C) 2006 Martin Willi - Hochschule fuer Technik Rapperswil
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
 * RCSID $Id: starterstroke.h 3825 2008-04-17 15:01:57Z martin $
 */

#ifndef _STARTER_STROKE_H_
#define _STARTER_STROKE_H_

#include "confread.h"

extern int starter_stroke_add_conn(starter_config_t *cfg, starter_conn_t *conn);
extern int starter_stroke_del_conn(starter_conn_t *conn);
extern int starter_stroke_route_conn(starter_conn_t *conn);
extern int starter_stroke_initiate_conn(starter_conn_t *conn);
extern int starter_stroke_add_ca(starter_ca_t *ca);
extern int starter_stroke_del_ca(starter_ca_t *ca);
extern int starter_stroke_configure(starter_config_t *cfg);

#endif /* _STARTER_STROKE_H_ */
