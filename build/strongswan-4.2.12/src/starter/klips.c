/* strongSwan KLIPS starter
 * Copyright (C) 2001-2002 Mathieu Lafon - Arkoon Network Security
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
 * RCSID $Id: klips.c 4632 2008-11-11 18:37:19Z martin $
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>

#include <freeswan.h>

#include "../pluto/constants.h"
#include "../pluto/defs.h"
#include "../pluto/log.h"

#include "files.h"

bool
starter_klips_init(void)
{
    struct stat stb;

    if (stat(PROC_KLIPS, &stb) != 0)
    {
	/* ipsec module makes the pf_key proc interface visible */
	if (stat(PROC_MODULES, &stb) == 0)
	{
	    ignore_result(system("modprobe -qv ipsec"));
	}

	/* now test again */
	if (stat(PROC_KLIPS, &stb) != 0)
	{
	    DBG(DBG_CONTROL,
		DBG_log("kernel appears to lack the KLIPS IPsec stack")
	    )
	    return FALSE;
	}
    }
    
    /* load crypto algorithm modules */
    ignore_result(system("modprobe -qv ipsec_aes"));
    ignore_result(system("modprobe -qv ipsec_blowfish"));
    ignore_result(system("modprobe -qv ipsec_sha2"));

    DBG(DBG_CONTROL,
	DBG_log("Found KLIPS IPsec stack")
    )
    
    return TRUE;
}

void
starter_klips_cleanup(void)
{
    if (system("type eroute > /dev/null 2>&1") == 0)
    {
	ignore_result(system("spi --clear"));
	ignore_result(system("eroute --clear"));
    }
	else if (system("type setkey > /dev/null 2>&1") == 0)
    {
	ignore_result(system("setkey -F"));
	ignore_result(system("setkey -FP"));
    }
    else
    {
	plog("WARNING: cannot flush IPsec state/policy database");
    }
}

