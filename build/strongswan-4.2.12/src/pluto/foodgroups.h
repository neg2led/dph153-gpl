/* Implement policygroups-style control files (aka "foodgroups")
 * Copyright (C) 2002  D. Hugh Redelmeier.
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
 * RCSID $Id: foodgroups.h 3252 2007-10-06 21:24:50Z andreas $
 */

struct connection;	/* forward declaration */
extern void add_group(struct connection *c);
extern void route_group(struct connection *c);
extern void unroute_group(struct connection *c);
extern void delete_group(const struct connection *c);

extern const char *policygroups_dir;
extern void load_groups(void);
