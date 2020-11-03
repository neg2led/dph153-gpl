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
 * $Id: x509_plugin.c 3640 2008-03-21 10:52:11Z andreas $
 */

#include "x509_plugin.h"

#include <library.h>
#include "x509_cert.h"
#include "x509_ac.h"
#include "x509_crl.h"
#include "x509_ocsp_request.h"
#include "x509_ocsp_response.h"

typedef struct private_x509_plugin_t private_x509_plugin_t;

/**
 * private data of x509_plugin
 */
struct private_x509_plugin_t {

	/**
	 * public functions
	 */
	x509_plugin_t public;
};

/**
 * Implementation of x509_plugin_t.x509troy
 */
static void destroy(private_x509_plugin_t *this)
{
	lib->creds->remove_builder(lib->creds,
							   (builder_constructor_t)x509_cert_builder);
	lib->creds->remove_builder(lib->creds,
							   (builder_constructor_t)x509_ac_builder);
	lib->creds->remove_builder(lib->creds,
							   (builder_constructor_t)x509_crl_builder);
	lib->creds->remove_builder(lib->creds,
							   (builder_constructor_t)x509_ocsp_request_builder);
	lib->creds->remove_builder(lib->creds,
							   (builder_constructor_t)x509_ocsp_response_builder);
	free(this);
}

/*
 * see header file
 */
plugin_t *plugin_create()
{
	private_x509_plugin_t *this = malloc_thing(private_x509_plugin_t);
	
	this->public.plugin.destroy = (void(*)(plugin_t*))destroy;

	lib->creds->add_builder(lib->creds, CRED_CERTIFICATE, CERT_X509,
							(builder_constructor_t)x509_cert_builder);
	lib->creds->add_builder(lib->creds, CRED_CERTIFICATE, CERT_X509_AC,
							(builder_constructor_t)x509_ac_builder);
	lib->creds->add_builder(lib->creds, CRED_CERTIFICATE, CERT_X509_CRL,
							(builder_constructor_t)x509_crl_builder);
	lib->creds->add_builder(lib->creds, CRED_CERTIFICATE, CERT_X509_OCSP_REQUEST,
							(builder_constructor_t)x509_ocsp_request_builder);
	lib->creds->add_builder(lib->creds, CRED_CERTIFICATE, CERT_X509_OCSP_RESPONSE,
							(builder_constructor_t)x509_ocsp_response_builder);

	return &this->public.plugin;
}

