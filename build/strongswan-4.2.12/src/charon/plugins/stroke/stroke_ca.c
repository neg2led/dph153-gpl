/*
 * Copyright (C) 2008 Tobias Brunner
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

#include "stroke_ca.h"
#include "stroke_cred.h"

#include <utils/mutex.h>
#include <utils/linked_list.h>
#include <crypto/hashers/hasher.h>

#include <daemon.h>

typedef struct private_stroke_ca_t private_stroke_ca_t;

/**
 * private data of stroke_ca
 */
struct private_stroke_ca_t {

	/**
	 * public functions
	 */
	stroke_ca_t public;
	
	/**
	 * read-write lock to lists
	 */
	rwlock_t *lock;
	
	/**
	 * list of starters CA sections and its certificates (ca_section_t)
	 */
	linked_list_t *sections;
	
	/**
	 * stroke credentials, stores our CA certificates
	 */
	stroke_cred_t *cred;
};

typedef struct ca_section_t ca_section_t;

/**
 * loaded ipsec.conf CA sections
 */
struct ca_section_t {

	/**
	 * name of the CA section
	 */
	char *name;
	
	/**
	 * reference to cert in trusted_credential_t
	 */
	certificate_t *cert;
	
	/**
	 * CRL URIs
	 */
	linked_list_t *crl;
	
	/**
	 * OCSP URIs
	 */
	linked_list_t *ocsp;
	
	/**
	 * Hashes of certificates issued by this CA
	 */
	linked_list_t *hashes;
	
	/**
	 * Base URI used for certificates from this CA
	 */
	char *certuribase;
};

/**
 * create a new CA section 
 */
static ca_section_t *ca_section_create(char *name, certificate_t *cert)
{
	ca_section_t *ca = malloc_thing(ca_section_t);
	
	ca->name = strdup(name);
	ca->crl = linked_list_create();
	ca->ocsp = linked_list_create();
	ca->cert = cert;
	ca->hashes = linked_list_create();
	ca->certuribase = NULL;
	return ca;
}

/**
 * destroy a ca section entry
 */
static void ca_section_destroy(ca_section_t *this)
{
	this->crl->destroy_function(this->crl, free);
	this->ocsp->destroy_function(this->ocsp, free);
	this->hashes->destroy_offset(this->hashes, offsetof(identification_t, destroy));
	free(this->certuribase);
	free(this->name);
	free(this);
}

/**
 * data to pass to create_inner_cdp
 */
typedef struct {
	private_stroke_ca_t *this;
	certificate_type_t type;
	identification_t *id;
} cdp_data_t;

/**
 * destroy cdp enumerator data and unlock list
 */
static void cdp_data_destroy(cdp_data_t *data)
{
	data->this->lock->unlock(data->this->lock);
	free(data);
}

/**
 * inner enumerator constructor for CDP URIs
 */
static enumerator_t *create_inner_cdp(ca_section_t *section, cdp_data_t *data)
{
	public_key_t *public;
	identification_t *keyid;
	enumerator_t *enumerator = NULL;
	linked_list_t *list;
	
	if (data->type == CERT_X509_OCSP_RESPONSE)
	{
		list = section->ocsp;
	}
	else
	{
		list = section->crl;
	}

	public = section->cert->get_public_key(section->cert);
	if (public)
	{
		if (!data->id)
		{
			enumerator = list->create_enumerator(list);
		}
		else
		{
			keyid = public->get_id(public, data->id->get_type(data->id));
			if (keyid && keyid->matches(keyid, data->id))
			{
				enumerator = list->create_enumerator(list);		
			}
		}
		public->destroy(public);
	}
	return enumerator;
}

/**
 * inner enumerator constructor for "Hash and URL"
 */
static enumerator_t *create_inner_cdp_hashandurl(ca_section_t *section, cdp_data_t *data)
{
	enumerator_t *enumerator = NULL, *hash_enum;
	identification_t *current;
	
	if (!data->id || !section->certuribase)
	{
		return NULL;
	}
	
	hash_enum = section->hashes->create_enumerator(section->hashes);
	while (hash_enum->enumerate(hash_enum, &current))
	{	
		if (current->matches(current, data->id))
		{
			char *url, *hash;
			
			url = malloc(strlen(section->certuribase) + 40 + 1);
			strcpy(url, section->certuribase);
			hash = chunk_to_hex(current->get_encoding(current), NULL, FALSE).ptr;
			strncat(url, hash, 40);
			free(hash);
			
			enumerator = enumerator_create_single(url, free);
			break;
		}
	}
	hash_enum->destroy(hash_enum);
	return enumerator;
}

/**
 * Implementation of credential_set_t.create_cdp_enumerator.
 */
static enumerator_t *create_cdp_enumerator(private_stroke_ca_t *this,
								certificate_type_t type, identification_t *id)
{
	cdp_data_t *data;

	switch (type)
	{	/* we serve CRLs, OCSP responders and URLs for "Hash and URL" */
		case CERT_X509:
		case CERT_X509_CRL:
		case CERT_X509_OCSP_RESPONSE:
		case CERT_ANY:
			break;
		default:
			return NULL;
	}
	data = malloc_thing(cdp_data_t);
	data->this = this;
	data->type = type;
	data->id = id;
	
	this->lock->read_lock(this->lock);
	return enumerator_create_nested(this->sections->create_enumerator(this->sections),
			(type == CERT_X509) ? (void*)create_inner_cdp_hashandurl : (void*)create_inner_cdp,
			data, (void*)cdp_data_destroy);
}
/**
 * Implementation of stroke_ca_t.add.
 */
static void add(private_stroke_ca_t *this, stroke_msg_t *msg)
{
	certificate_t *cert;
	ca_section_t *ca;
	
	if (msg->add_ca.cacert == NULL)
	{
		DBG1(DBG_CFG, "missing cacert parameter");
		return;
	}	
	cert = this->cred->load_ca(this->cred, msg->add_ca.cacert);
	if (cert)
	{
		ca = ca_section_create(msg->add_ca.name, cert);
		if (msg->add_ca.crluri)
		{
			ca->crl->insert_last(ca->crl, strdup(msg->add_ca.crluri));
		}
		if (msg->add_ca.crluri2)
		{
			ca->crl->insert_last(ca->crl, strdup(msg->add_ca.crluri2));
		}
		if (msg->add_ca.ocspuri)
		{
			ca->ocsp->insert_last(ca->ocsp, strdup(msg->add_ca.ocspuri));
		}
		if (msg->add_ca.ocspuri2)
		{
			ca->ocsp->insert_last(ca->ocsp, strdup(msg->add_ca.ocspuri2));
		}
		if (msg->add_ca.certuribase)
		{
			ca->certuribase = strdup(msg->add_ca.certuribase);
		}
		this->lock->write_lock(this->lock);
		this->sections->insert_last(this->sections, ca);
		this->lock->unlock(this->lock);
		DBG1(DBG_CFG, "added ca '%s'", msg->add_ca.name);
	}
}

/**
 * Implementation of stroke_ca_t.del.
 */
static void del(private_stroke_ca_t *this, stroke_msg_t *msg)
{
	enumerator_t *enumerator;
	ca_section_t *ca = NULL;
	
	this->lock->write_lock(this->lock);
	enumerator = this->sections->create_enumerator(this->sections);
	while (enumerator->enumerate(enumerator, &ca))
	{
		if (streq(ca->name, msg->del_ca.name))
		{
			this->sections->remove_at(this->sections, enumerator);
			break;
		}
		ca = NULL;
	}
	enumerator->destroy(enumerator);
	this->lock->unlock(this->lock);
	if (ca == NULL)
	{
		DBG1(DBG_CFG, "no ca named '%s' found\n", msg->del_ca.name);
		return;
	}
	ca_section_destroy(ca);
	/* TODO: flush cached certs */
}

/**
 * list crl or ocsp URIs
 */
static void list_uris(linked_list_t *list, char *label, FILE *out)
{
	bool first = TRUE;
	char *uri;
	enumerator_t *enumerator;

	enumerator = list->create_enumerator(list);
	while (enumerator->enumerate(enumerator, (void**)&uri))
	{
		if (first)
		{
			fprintf(out, label);
			first = FALSE;
		}
		else
		{
			fprintf(out, "            ");
		}
		fprintf(out, "'%s'\n", uri);
	}
	enumerator->destroy(enumerator);
}

/**
 * Implementation of stroke_ca_t.check_for_hash_and_url.
 */
static void check_for_hash_and_url(private_stroke_ca_t *this, certificate_t* cert)
{
	ca_section_t *section;
	enumerator_t *enumerator;
	
	hasher_t *hasher = lib->crypto->create_hasher(lib->crypto, HASH_SHA1);
	if (hasher == NULL)
	{
		DBG1(DBG_IKE, "unable to use hash-and-url: sha1 not supported");
		return;
	}
	
	this->lock->write_lock(this->lock);
	enumerator = this->sections->create_enumerator(this->sections);
	while (enumerator->enumerate(enumerator, (void**)&section))
	{
		if (section->certuribase && cert->issued_by(cert, section->cert))
		{
			chunk_t hash, encoded = cert->get_encoding(cert);
			hasher->allocate_hash(hasher, encoded, &hash);
			section->hashes->insert_last(section->hashes,
					identification_create_from_encoding(ID_CERT_DER_SHA1, hash));
			chunk_free(&hash);
			chunk_free(&encoded);
			break;
		}
	}
	enumerator->destroy(enumerator);
	this->lock->unlock(this->lock);
	
	hasher->destroy(hasher);
}

/**
 * Implementation of stroke_ca_t.list.
 */
static void list(private_stroke_ca_t *this, stroke_msg_t *msg, FILE *out)
{
	bool first = TRUE;
	ca_section_t *section;
	enumerator_t *enumerator;
	
	this->lock->read_lock(this->lock);
	enumerator = this->sections->create_enumerator(this->sections);
	while (enumerator->enumerate(enumerator, (void**)&section))
	{
		certificate_t *cert = section->cert;
		public_key_t *public = cert->get_public_key(cert);

		if (first)
		{
			fprintf(out, "\n");
			fprintf(out, "List of CA Information Sections:\n");
			first = FALSE;
		}
		fprintf(out, "\n");
		fprintf(out, "  authname:    \"%D\"\n", cert->get_subject(cert));

		/* list authkey and keyid */
		if (public)
		{
			fprintf(out, "  authkey:      %D\n",
					public->get_id(public, ID_PUBKEY_SHA1));
			fprintf(out, "  keyid:        %D\n",
					public->get_id(public, ID_PUBKEY_INFO_SHA1));
			public->destroy(public);
		}
		list_uris(section->crl, "  crluris:     ", out);
		list_uris(section->ocsp, "  ocspuris:    ", out);
		if (section->certuribase)
		{
			fprintf(out, "  certuribase: '%s'\n", section->certuribase);
		}
	}
	enumerator->destroy(enumerator);
	this->lock->unlock(this->lock);
}

/**
 * Implementation of stroke_ca_t.destroy
 */
static void destroy(private_stroke_ca_t *this)
{
	this->sections->destroy_function(this->sections, (void*)ca_section_destroy);
	this->lock->destroy(this->lock);
	free(this);
}

/*
 * see header file
 */
stroke_ca_t *stroke_ca_create(stroke_cred_t *cred)
{
	private_stroke_ca_t *this = malloc_thing(private_stroke_ca_t);
	
	this->public.set.create_private_enumerator = (void*)return_null;
	this->public.set.create_cert_enumerator = (void*)return_null;
	this->public.set.create_shared_enumerator = (void*)return_null;
	this->public.set.create_cdp_enumerator = (void*)create_cdp_enumerator;
	this->public.set.cache_cert = (void*)nop;
	this->public.add = (void(*)(stroke_ca_t*, stroke_msg_t *msg))add;
	this->public.del = (void(*)(stroke_ca_t*, stroke_msg_t *msg))del;
	this->public.list = (void(*)(stroke_ca_t*, stroke_msg_t *msg, FILE *out))list;
	this->public.check_for_hash_and_url = (void(*)(stroke_ca_t*, certificate_t*))check_for_hash_and_url;
	this->public.destroy = (void(*)(stroke_ca_t*))destroy;
	
	this->sections = linked_list_create();
	this->lock = rwlock_create(RWLOCK_DEFAULT);
	this->cred = cred;
	
	return &this->public;
}

