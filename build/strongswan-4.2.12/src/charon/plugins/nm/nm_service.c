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
 *
 * $Id$
 */

#include <nm-setting-vpn.h>
#include "nm_service.h"

#include <daemon.h>
#include <asn1/pem.h>
#include <utils/host.h>
#include <utils/identification.h>
#include <config/peer_cfg.h>

#include <stdio.h>

#define CONFIG_NAME "NetworkManager"

G_DEFINE_TYPE(NMStrongswanPlugin, nm_strongswan_plugin, NM_TYPE_VPN_PLUGIN)

/**
 * Private data of NMStrongswanPlugin
 */
typedef struct {
	listener_t listener;
	ike_sa_t *ike_sa;
	NMVPNPlugin *plugin;
	nm_creds_t *creds;
} NMStrongswanPluginPrivate;

#define NM_STRONGSWAN_PLUGIN_GET_PRIVATE(o) \
			(G_TYPE_INSTANCE_GET_PRIVATE ((o), \
				NM_TYPE_STRONGSWAN_PLUGIN, NMStrongswanPluginPrivate))

/**
 * signal IPv4 config to NM, set connection as established
 */
static void signal_ipv4_config(NMVPNPlugin *plugin,
							   ike_sa_t *ike_sa, child_sa_t *child_sa)
{
	GValue *val;
	GHashTable *config;
	host_t *me, *other;
	
	config = g_hash_table_new(g_str_hash, g_str_equal);
	me = ike_sa->get_my_host(ike_sa);
	other = ike_sa->get_other_host(ike_sa);
	
	/* NM requires a tundev, but netkey does not use one. Passing an invalid
	 * iface makes NM complain, but it accepts it without fiddling on eth0. */
	val = g_slice_new0 (GValue);
	g_value_init (val, G_TYPE_STRING);
	g_value_set_string (val, "none");
	g_hash_table_insert (config, NM_VPN_PLUGIN_IP4_CONFIG_TUNDEV, val);
	
	val = g_slice_new0(GValue);
	g_value_init(val, G_TYPE_UINT);
	g_value_set_uint(val, *(u_int32_t*)me->get_address(me).ptr);
	g_hash_table_insert(config, NM_VPN_PLUGIN_IP4_CONFIG_ADDRESS, val);
	
	val = g_slice_new0(GValue);
	g_value_init(val, G_TYPE_UINT);
	g_value_set_uint(val, me->get_address(me).len * 8);
	g_hash_table_insert(config, NM_VPN_PLUGIN_IP4_CONFIG_PREFIX, val);
	
	nm_vpn_plugin_set_ip4_config(plugin, config);
}

/**
 * signal failure to NM, connecting failed
 */
static void signal_failure(NMVPNPlugin *plugin)
{
	/* TODO: NM does not handle this failure!? 
	nm_vpn_plugin_failure(plugin, NM_VPN_PLUGIN_FAILURE_LOGIN_FAILED); */
	nm_vpn_plugin_set_state(plugin, NM_VPN_SERVICE_STATE_STOPPED);
}

/**
 * Implementation of listener_t.ike_state_change
 */
static bool ike_state_change(listener_t *listener, ike_sa_t *ike_sa,
							 ike_sa_state_t state)
{
	NMStrongswanPluginPrivate *private = (NMStrongswanPluginPrivate*)listener;
	
	if (private->ike_sa == ike_sa)
	{
		switch (state)
		{
			case IKE_DESTROYING:
				signal_failure(private->plugin);
				return FALSE;
			default:
				break;
		}
	}
	return TRUE;
}

/**
 * Implementation of listener_t.child_state_change
 */
static bool child_state_change(listener_t *listener, ike_sa_t *ike_sa,
							   child_sa_t *child_sa, child_sa_state_t state)
{
	NMStrongswanPluginPrivate *private = (NMStrongswanPluginPrivate*)listener;

	if (private->ike_sa == ike_sa)
	{
		switch (state)
		{
			case CHILD_INSTALLED:
				signal_ipv4_config(private->plugin, ike_sa, child_sa);
				return FALSE;
			case CHILD_DESTROYING:
				signal_failure(private->plugin);
				return FALSE;
			default:
				break;
		}
	}
	return TRUE;
}

/**
 * Connect function called from NM via DBUS
 */
static gboolean connect_(NMVPNPlugin *plugin, NMConnection *connection,
						 GError **err)
{
	nm_creds_t *creds;
	NMSettingVPN *settings;
	identification_t *user = NULL, *gateway;
	char *address, *str;
	bool virtual, encap, ipcomp;
	ike_cfg_t *ike_cfg;
	peer_cfg_t *peer_cfg;
	child_cfg_t *child_cfg;
	traffic_selector_t *ts;
	ike_sa_t *ike_sa;
	auth_info_t *auth;
	auth_class_t auth_class = AUTH_CLASS_EAP;
	certificate_t *cert = NULL;
	bool agent = FALSE;
	
	/**
	 * Read parameters
	 */
	settings = NM_SETTING_VPN(nm_connection_get_setting(connection,
														NM_TYPE_SETTING_VPN));
	
	DBG4(DBG_CFG, "received NetworkManager connection: %s",
		 nm_setting_to_string(NM_SETTING(settings)));
	address = g_hash_table_lookup(settings->data, "address");
	if (!address || !*address)
	{
		g_set_error(err, NM_VPN_PLUGIN_ERROR, NM_VPN_PLUGIN_ERROR_BAD_ARGUMENTS,
				    "Gateway address missing.");
		return FALSE;
	}
	str = g_hash_table_lookup(settings->data, "virtual");
	virtual = str && streq(str, "yes");
	str = g_hash_table_lookup(settings->data, "encap");
	encap = str && streq(str, "yes");
	str = g_hash_table_lookup(settings->data, "ipcomp");
	ipcomp = str && streq(str, "yes");
	str = g_hash_table_lookup(settings->data, "method");
	if (str)
	{
		if (streq(str, "psk"))
		{
			auth_class = AUTH_CLASS_PSK;
		}
		else if (streq(str, "agent"))
		{
			auth_class = AUTH_CLASS_PUBKEY;
			agent = TRUE;
		}
		else if (streq(str, "key"))
		{
			auth_class = AUTH_CLASS_PUBKEY;
		}
	}
	
	/**
	 * Register credentials
	 */
	creds = NM_STRONGSWAN_PLUGIN_GET_PRIVATE(plugin)->creds;
	creds->clear(creds);
	
	/* gateway cert */
	str = g_hash_table_lookup(settings->data, "certificate");
	if (str)
	{
		cert = lib->creds->create(lib->creds, CRED_CERTIFICATE, CERT_X509,
								  BUILD_FROM_FILE, str, BUILD_END);
		creds->set_certificate(creds, cert);
	}
	if (!cert)
	{
		g_set_error(err, NM_VPN_PLUGIN_ERROR, NM_VPN_PLUGIN_ERROR_BAD_ARGUMENTS,
				    "Loading gateway certificate failed.");
		return FALSE;
	}
	gateway = cert->get_subject(cert);
	
	if (auth_class == AUTH_CLASS_EAP)
	{
		/* username/password authentication ... */
		str = g_hash_table_lookup(settings->data, "user");
		if (str)
		{
			user = identification_create_from_encoding(ID_KEY_ID,
											chunk_create(str, strlen(str)));
			str = g_hash_table_lookup(settings->secrets, "password");
			creds->set_username_password(creds, user, str);
		}
	}
	
	if (auth_class == AUTH_CLASS_PUBKEY)
	{
		/* ... or certificate/private key authenitcation */
		str = g_hash_table_lookup(settings->data, "usercert");
		if (str)
		{
			public_key_t *public;
			private_key_t *private = NULL;
			
			cert = lib->creds->create(lib->creds, CRED_CERTIFICATE, CERT_X509,
									  BUILD_FROM_FILE, str, BUILD_END);	
									  
			/* try agent */  
			str = g_hash_table_lookup(settings->secrets, "agent");
			if (agent && str && cert)
			{
				public = cert->get_public_key(cert);
				if (public)
				{
					private = lib->creds->create(lib->creds, CRED_PRIVATE_KEY,
												 public->get_type(public),
												 BUILD_AGENT_SOCKET, str,
												 BUILD_PUBLIC_KEY, public,
												 BUILD_END);
					public->destroy(public);
				}
			}
			/* ... or key file */  
			str = g_hash_table_lookup(settings->data, "userkey");
			if (!agent && str && cert)
			{
				chunk_t secret, chunk;
				bool pgp = FALSE;
				
				secret.ptr = g_hash_table_lookup(settings->secrets, "password");
				if (secret.ptr)
				{
					secret.len = strlen(secret.ptr);
				}
				if (pem_asn1_load_file(str, &secret, &chunk, &pgp))
				{
					private = lib->creds->create(lib->creds, CRED_PRIVATE_KEY,
								KEY_RSA, BUILD_BLOB_ASN1_DER, chunk, BUILD_END);
					free(chunk.ptr);
				}
			}
			if (private)
			{
				user = cert->get_subject(cert);
				user = user->clone(user);
				creds->set_cert_and_key(creds, cert, private);
			}
			else
			{
				DESTROY_IF(cert);
				g_set_error(err, NM_VPN_PLUGIN_ERROR, NM_VPN_PLUGIN_ERROR_BAD_ARGUMENTS,
							"Loading user certificate/private key failed.");
				return FALSE;
			}
		}
	}
	
	if (!user)
	{
		g_set_error(err, NM_VPN_PLUGIN_ERROR, NM_VPN_PLUGIN_ERROR_BAD_ARGUMENTS,
					"Configuration parameters missing.");
		return FALSE;
	}
	
	/**
	 * Set up configurations
	 */
	ike_cfg = ike_cfg_create(TRUE, encap, "0.0.0.0", address);
	ike_cfg->add_proposal(ike_cfg, proposal_create_default(PROTO_IKE));
	peer_cfg = peer_cfg_create(CONFIG_NAME, 2, ike_cfg,
					user, gateway->clone(gateway),
					CERT_SEND_IF_ASKED, UNIQUE_REPLACE, 1, /* keyingtries */
					36000, 0, /* rekey 10h, reauth none */
					600, 600, /* jitter, over 10min */
					TRUE, 0, /* mobike, DPD */
					virtual ? host_create_from_string("0.0.0.0", 0) : NULL,
					NULL, FALSE, NULL, NULL); /* pool, mediation */
	auth = peer_cfg->get_auth(peer_cfg);
	auth->add_item(auth, AUTHN_AUTH_CLASS, &auth_class);
	child_cfg = child_cfg_create(CONFIG_NAME,
								 10800, 10200, /* lifetime 3h, rekey 2h50min */
								 300, /* jitter 5min */
								 NULL, TRUE, MODE_TUNNEL, /* updown, hostaccess */
								 ACTION_NONE, ACTION_RESTART, ipcomp);
	child_cfg->add_proposal(child_cfg, proposal_create_default(PROTO_ESP));
	ts = traffic_selector_create_dynamic(0, 0, 65535);
	child_cfg->add_traffic_selector(child_cfg, TRUE, ts);
	ts = traffic_selector_create_from_string(0, TS_IPV4_ADDR_RANGE,
											 "0.0.0.0", 0,
											 "255.255.255.255", 65535);
	child_cfg->add_traffic_selector(child_cfg, FALSE, ts);
	peer_cfg->add_child_cfg(peer_cfg, child_cfg);
	
	/**
	 * Start to initiate
	 */
	ike_sa = charon->ike_sa_manager->checkout_by_config(charon->ike_sa_manager,
														peer_cfg);
	if (!ike_sa->get_peer_cfg(ike_sa))
	{
		ike_sa->set_peer_cfg(ike_sa, peer_cfg);
	}
	else
	{
		peer_cfg->destroy(peer_cfg);
	}
	if (ike_sa->initiate(ike_sa, child_cfg) != SUCCESS)
	{
		charon->ike_sa_manager->checkin_and_destroy(charon->ike_sa_manager, ike_sa);
		
		g_set_error(err, NM_VPN_PLUGIN_ERROR, NM_VPN_PLUGIN_ERROR_LAUNCH_FAILED,
				    "Initiating failed.");
		return FALSE;
	}
	
	/**
	 * Register listener
	 */
	NM_STRONGSWAN_PLUGIN_GET_PRIVATE(plugin)->ike_sa = ike_sa;
	charon->bus->add_listener(charon->bus, 
							&NM_STRONGSWAN_PLUGIN_GET_PRIVATE(plugin)->listener);
	charon->ike_sa_manager->checkin(charon->ike_sa_manager, ike_sa);
	return TRUE;
}

/**
 * NeedSecrets called from NM via DBUS 
 */
static gboolean need_secrets(NMVPNPlugin *plugin, NMConnection *connection,
							 char **setting_name, GError **error)
{
	NMSettingVPN *settings;
	char *method, *path;
	chunk_t secret = chunk_empty, key;
	bool pgp = FALSE;
	
	settings = NM_SETTING_VPN(nm_connection_get_setting(connection,
														NM_TYPE_SETTING_VPN));
	method = g_hash_table_lookup(settings->data, "method");
	if (method)
	{
		if (streq(method, "eap"))
		{
			if (g_hash_table_lookup(settings->secrets, "password"))
			{
				return FALSE;
			}
		}
		else if (streq(method, "agent"))
		{
			if (g_hash_table_lookup(settings->secrets, "agent"))
			{
				return FALSE;
			}
		}
		else if (streq(method, "key"))
		{
			path = g_hash_table_lookup(settings->data, "userkey");
			if (path)
			{
				secret.ptr = g_hash_table_lookup(settings->secrets, "password");
				if (secret.ptr)
				{
					secret.len = strlen(secret.ptr);
				}
				if (pem_asn1_load_file(path, &secret, &key, &pgp))
				{
					free(key.ptr);
					return FALSE;
				}
			}
		}
	}
	*setting_name = NM_SETTING_VPN_SETTING_NAME;
	return TRUE;
}

/**
 * Disconnect called from NM via DBUS 
 */
static gboolean disconnect(NMVPNPlugin *plugin, GError **err)
{
	enumerator_t *enumerator;
	ike_sa_t *ike_sa;
	u_int id;
	
	enumerator = charon->controller->create_ike_sa_enumerator(charon->controller);
	while (enumerator->enumerate(enumerator, &ike_sa))
	{
		if (streq(CONFIG_NAME, ike_sa->get_name(ike_sa)))
		{
			id = ike_sa->get_unique_id(ike_sa);
			enumerator->destroy(enumerator);
			charon->controller->terminate_ike(charon->controller, id,
											  controller_cb_empty, NULL);
			return TRUE;
		}
	}
	enumerator->destroy(enumerator);
	return FALSE;
}

/**
 * Initializer
 */
static void nm_strongswan_plugin_init(NMStrongswanPlugin *plugin)
{
	NMStrongswanPluginPrivate *private;
	
	private = NM_STRONGSWAN_PLUGIN_GET_PRIVATE(plugin);
	private->plugin = NM_VPN_PLUGIN(plugin);
	memset(&private->listener.log, 0, sizeof(listener_t));
	private->listener.ike_state_change = ike_state_change;
	private->listener.child_state_change = child_state_change;
}

/**
 * Class constructor
 */
static void nm_strongswan_plugin_class_init(
									NMStrongswanPluginClass *strongswan_class)
{
	NMVPNPluginClass *parent_class = NM_VPN_PLUGIN_CLASS(strongswan_class);
	
	g_type_class_add_private(G_OBJECT_CLASS(strongswan_class),
							 sizeof(NMStrongswanPluginPrivate));
	parent_class->connect = connect_;
	parent_class->need_secrets = need_secrets;
	parent_class->disconnect = disconnect;
}

/**
 * Object constructor
 */
NMStrongswanPlugin *nm_strongswan_plugin_new(nm_creds_t *creds)
{
	NMStrongswanPlugin *plugin = (NMStrongswanPlugin *)g_object_new (
					NM_TYPE_STRONGSWAN_PLUGIN,
					NM_VPN_PLUGIN_DBUS_SERVICE_NAME, NM_DBUS_SERVICE_STRONGSWAN,
					NULL);
	if (plugin)
	{
		NM_STRONGSWAN_PLUGIN_GET_PRIVATE(plugin)->creds = creds;
	}
	return plugin;
}

