/* strongSwan IPsec config file parser
 * Copyright (C) 2001-2002 Mathieu Lafon
 * Arkoon Network Security
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
 * RCSID $Id: confread.h 4612 2008-11-11 06:37:37Z andreas $
 */

#ifndef _IPSEC_CONFREAD_H_
#define _IPSEC_CONFREAD_H_

#ifndef _FREESWAN_H
#include <freeswan.h>
#include "../pluto/constants.h"
#endif

#include "parser.h"
#include "interfaces.h"

typedef enum {
	STARTUP_NO,
	STARTUP_ADD,
	STARTUP_ROUTE,
	STARTUP_START
} startup_t;

typedef enum {
	STATE_IGNORE,
	STATE_TO_ADD,
	STATE_ADDED,
	STATE_REPLACED,
	STATE_INVALID
} starter_state_t;

typedef enum {
	KEY_EXCHANGE_IKE,
	KEY_EXCHANGE_IKEV1,
	KEY_EXCHANGE_IKEV2
} keyexchange_t;

typedef enum {
	STRICT_NO,
	STRICT_YES,
	STRICT_IFURI
} strict_t;

typedef struct starter_end starter_end_t;

struct starter_end {
	lset_t		seen;
	char		*id;
	char		*rsakey;
	char		*cert;
	char		*ca;
	char		*groups;
	char		*iface;
	ip_address	addr;
	ip_address	nexthop;
	char 		*subnet;
	bool		has_client;
	bool		has_client_wildcard;
	bool		has_port_wildcard;
	bool		has_natip;
	bool		has_virt;
	bool		modecfg;
	certpolicy_t	sendcert;
	bool		firewall;
	bool		hostaccess;
	bool		allow_any;
	bool		dns_failed;
	char 		*updown;
	u_int16_t	port;
	u_int8_t	protocol;
	char		*srcip;
};

typedef struct also also_t;

struct also {
	char		*name;
	bool		included;
	also_t		*next;
};

typedef struct starter_conn starter_conn_t;

struct starter_conn {
	lset_t		seen;
	char		*name;
	also_t		*also;
	kw_list_t	*kw;
	u_int		visit;
	startup_t	startup;
	starter_state_t	state;

	keyexchange_t	keyexchange;
	u_int32_t	eap_type;
	u_int32_t	eap_vendor;
	char		*eap_identity;
	lset_t 		policy;
	time_t 		sa_ike_life_seconds;
	time_t		sa_ipsec_life_seconds;
	time_t		sa_rekey_margin;
	unsigned long	sa_keying_tries;
	unsigned long	sa_rekey_fuzz;
	sa_family_t	addr_family;
	sa_family_t	tunnel_addr_family;
	bool		install_policy;
	starter_end_t	left, right;

	unsigned long	id;

	char		*esp;
	char 		*ike;
	char 		*pfsgroup;
	
	time_t		dpd_delay;
	time_t		dpd_timeout;
	dpd_action_t	dpd_action;
	int		dpd_count;
	
	bool		me_mediation;
	char		*me_mediated_by;
	char		*me_peerid;

	starter_conn_t *next;
};

typedef struct starter_ca starter_ca_t;

struct starter_ca {
	lset_t		seen;
	char 		*name;
	also_t		*also;
	kw_list_t	*kw;
	u_int		visit;
	startup_t	startup;
	starter_state_t	state;

	char		*cacert;
	char		*ldaphost;
	char		*ldapbase;
	char		*crluri;
	char		*crluri2;
	char		*ocspuri;
	char		*ocspuri2;
	char        *certuribase;

	bool		strict;

	starter_ca_t	*next;
};

typedef struct starter_config starter_config_t;

struct starter_config {
	struct {
		lset_t	seen;
		char	**interfaces;
		char	*dumpdir;
		bool	charonstart;
		bool	plutostart;

		/* pluto/charon keywords */
		char	 **plutodebug;
		char	 *charondebug;
		char	 *prepluto;
		char	 *postpluto;
		char	 *plutostderrlog;
		bool	 uniqueids;
		u_int	 overridemtu;
		u_int	 crlcheckinterval;
		bool	 cachecrls;
		strict_t strictcrlpolicy;
		bool	 nocrsend;
		bool	 nat_traversal;
		u_int	 keep_alive;
		u_int	 force_keepalive;
		char	 *virtual_private;
		char	 *pkcs11module;
		char	 *pkcs11initargs;
		bool	 pkcs11keepstate;
		bool	 pkcs11proxy;

		/* KLIPS keywords */
		char	**klipsdebug;
		bool	fragicmp;
		char	*packetdefault;
		bool	hidetos;
	} setup;

	/* information about the default route */
	defaultroute_t defaultroute;

	/* number of encountered parsing errors */
	u_int err;
	u_int non_fatal_err;

	/* do we parse also statements */
	bool parse_also;

	/* ca %default */
	starter_ca_t ca_default;

	/* connections list (without %default) */
	starter_ca_t *ca_first, *ca_last;

	/* conn %default */
	starter_conn_t conn_default;

	/* connections list (without %default) */
	starter_conn_t *conn_first, *conn_last;
};

extern starter_config_t *confread_load(const char *file);
extern void confread_free(starter_config_t *cfg);

#endif /* _IPSEC_CONFREAD_H_ */

