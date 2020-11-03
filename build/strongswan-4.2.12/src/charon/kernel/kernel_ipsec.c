/*
 * Copyright (C) 2008 Tobias Brunner
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
 * $Id: kernel_ipsec.c 4430 2008-10-14 08:46:31Z tobias $
 */

#include "kernel_ipsec.h"

ENUM(ipsec_mode_names, MODE_TRANSPORT, MODE_BEET,
	"TRANSPORT",
	"TUNNEL",
	"2",
	"3",
	"BEET",
);

ENUM(policy_dir_names, POLICY_IN, POLICY_FWD,
	"in",
	"out",
	"fwd"
);

