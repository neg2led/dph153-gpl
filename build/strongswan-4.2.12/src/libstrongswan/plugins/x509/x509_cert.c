/*
 * Copyright (C) 2000 Andreas Hess, Patric Lichtsteiner, Roger Wegmann
 * Copyright (C) 2001 Marco Bertossa, Andreas Schleiss
 * Copyright (C) 2002 Mario Strasser
 * Copyright (C) 2000-2006 Andreas Steffen
 * Copyright (C) 2006-2008 Martin Willi
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
 * $Id: x509_cert.c 4767 2008-12-08 19:15:38Z martin $
 */

#define _GNU_SOURCE

#include "x509_cert.h"

#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include <library.h>
#include <debug.h>
#include <asn1/oid.h>
#include <asn1/asn1.h>
#include <asn1/asn1_parser.h>
#include <asn1/pem.h>
#include <crypto/hashers/hasher.h>
#include <utils/linked_list.h>
#include <utils/identification.h>

/**
 * Different kinds of generalNames
 */
typedef enum {
	GN_OTHER_NAME =		0,
	GN_RFC822_NAME =	1,
	GN_DNS_NAME =		2,
	GN_X400_ADDRESS =	3,
	GN_DIRECTORY_NAME =	4,
	GN_EDI_PARTY_NAME = 5,
	GN_URI =			6,
	GN_IP_ADDRESS =		7,
	GN_REGISTERED_ID =	8,
} generalNames_t;


typedef struct private_x509_cert_t private_x509_cert_t;

/**
 * Private data of a x509_cert_t object.
 */
struct private_x509_cert_t {
	/**
	 * Public interface for this certificate.
	 */
	x509_cert_t public;
	
	/**
	 * X.509 certificate encoding in ASN.1 DER format
	 */
	chunk_t encoding;
	
	/**
	 * SHA1 hash of the DER encoding of this X.509 certificate
	 */
	chunk_t encoding_hash;
	
	/**
	 * X.509 certificate body over which signature is computed
	 */
	chunk_t tbsCertificate;

	/**
	 * Version of the X.509 certificate
	 */
	u_int version;
	
	/**
	 * Serial number of the X.509 certificate
	 */
	chunk_t serialNumber;
	
	/**
	 * ID representing the certificate issuer
	 */
	identification_t *issuer;
	
	/**
	 * Start time of certificate validity
	 */
	time_t notBefore;
	
	/**
	 * End time of certificate validity
	 */
	time_t notAfter;
	
	/**
	 * ID representing the certificate subject
	 */
	identification_t *subject;
	
	/**
	 * List of subjectAltNames as identification_t
	 */
	linked_list_t *subjectAltNames;
	
	/**
	 * List of crlDistributionPoints as allocated char*
	 */
	linked_list_t *crl_uris;
	
	/**
	 * List ocspAccessLocations as identification_t
	 */
	linked_list_t *ocsp_uris;
	
	/**
	 * certificates embedded public key
	 */
	public_key_t *public_key;
	
	/**
	 * Subject Key Identifier
	 */
	chunk_t subjectKeyID;
	
	/**
	 * Authority Key Identifier
	 */
	identification_t *authKeyIdentifier;
	
	/**
	 * Authority Key Serial Number
	 */
	chunk_t authKeySerialNumber;
	
	/**
	 * x509 constraints and other flags
	 */
	x509_flag_t flags;
	
	/**
	 * Signature algorithm
	 */
	int algorithm;
	
	/**
	 * Signature
	 */
	chunk_t signature;
	
	/**
	 * Certificate parsed from blob/file?
	 */
	bool parsed;
	
	/**
	 * reference count
	 */
	refcount_t ref;
};

static u_char ASN1_sAN_oid_buf[] = {
	0x06, 0x03, 0x55, 0x1D, 0x11
};
static const chunk_t ASN1_subjectAltName_oid = chunk_from_buf(ASN1_sAN_oid_buf);

/**
 * ASN.1 definition of a basicConstraints extension 
 */
static const asn1Object_t basicConstraintsObjects[] = {
	{ 0, "basicConstraints",	ASN1_SEQUENCE,	ASN1_NONE			}, /*  0 */
	{ 1,   "CA",				ASN1_BOOLEAN,	ASN1_DEF|ASN1_BODY	}, /*  1 */
	{ 1,   "pathLenConstraint",	ASN1_INTEGER,	ASN1_OPT|ASN1_BODY	}, /*  2 */
	{ 1,   "end opt",			ASN1_EOC,		ASN1_END  			}, /*  3 */
	{ 0, "exit",				ASN1_EOC,		ASN1_EXIT  			}
};
#define BASIC_CONSTRAINTS_CA	1

/**
 * Extracts the basicConstraints extension
 */
static bool parse_basicConstraints(chunk_t blob, int level0)
{
	asn1_parser_t *parser;
	chunk_t object;
	int objectID;
	bool isCA = FALSE;

	parser = asn1_parser_create(basicConstraintsObjects, blob);
	parser->set_top_level(parser, level0);

	while (parser->iterate(parser, &objectID, &object))
	{
		if (objectID == BASIC_CONSTRAINTS_CA)
		{
			isCA = object.len && *object.ptr;
			DBG2("  %s", isCA ? "TRUE" : "FALSE");
		}
	}
	parser->destroy(parser);

	return isCA;
}

/**
 * ASN.1 definition of otherName 
 */
static const asn1Object_t otherNameObjects[] = {
	{0, "type-id",	ASN1_OID,			ASN1_BODY	}, /* 0 */
	{0, "value",	ASN1_CONTEXT_C_0,	ASN1_BODY	}, /* 1 */
	{0, "exit",		ASN1_EOC,			ASN1_EXIT	}
};
#define ON_OBJ_ID_TYPE		0
#define ON_OBJ_VALUE		1

/**
 * Extracts an otherName
 */
static bool parse_otherName(chunk_t blob, int level0)
{
	asn1_parser_t *parser;
	chunk_t object;
	int objectID;
	int oid = OID_UNKNOWN;
	bool success = FALSE;

	parser = asn1_parser_create(otherNameObjects, blob);
	parser->set_top_level(parser, level0);

	while (parser->iterate(parser, &objectID, &object))
	{
		switch (objectID)
		{
			case ON_OBJ_ID_TYPE:
				oid = asn1_known_oid(object);
				break;
			case ON_OBJ_VALUE:
				if (oid == OID_XMPP_ADDR)
				{
					if (!asn1_parse_simple_object(&object, ASN1_UTF8STRING,
								parser->get_level(parser)+1, "xmppAddr"))
					{
						goto end;
					}
				}
				break;
			default:
				break;
		}
	}
	success = parser->success(parser);
	
end:
	parser->destroy(parser);
	return success;
}

/**
 * ASN.1 definition of generalName 
 */
static const asn1Object_t generalNameObjects[] = {
	{ 0, "otherName",		ASN1_CONTEXT_C_0,  ASN1_OPT|ASN1_BODY	}, /*  0 */
	{ 0, "end choice",		ASN1_EOC,          ASN1_END				}, /*  1 */
	{ 0, "rfc822Name",		ASN1_CONTEXT_S_1,  ASN1_OPT|ASN1_BODY	}, /*  2 */
	{ 0, "end choice",		ASN1_EOC,          ASN1_END 			}, /*  3 */
	{ 0, "dnsName",			ASN1_CONTEXT_S_2,  ASN1_OPT|ASN1_BODY	}, /*  4 */
	{ 0, "end choice",		ASN1_EOC,          ASN1_END				}, /*  5 */
	{ 0, "x400Address",		ASN1_CONTEXT_S_3,  ASN1_OPT|ASN1_BODY	}, /*  6 */
	{ 0, "end choice",		ASN1_EOC,          ASN1_END				}, /*  7 */
	{ 0, "directoryName",	ASN1_CONTEXT_C_4,  ASN1_OPT|ASN1_BODY	}, /*  8 */
	{ 0, "end choice",		ASN1_EOC,          ASN1_END				}, /*  9 */
	{ 0, "ediPartyName",	ASN1_CONTEXT_C_5,  ASN1_OPT|ASN1_BODY	}, /* 10 */
	{ 0, "end choice",		ASN1_EOC,          ASN1_END				}, /* 11 */
	{ 0, "URI",				ASN1_CONTEXT_S_6,  ASN1_OPT|ASN1_BODY	}, /* 12 */
	{ 0, "end choice",		ASN1_EOC,          ASN1_END				}, /* 13 */
	{ 0, "ipAddress",		ASN1_CONTEXT_S_7,  ASN1_OPT|ASN1_BODY	}, /* 14 */
	{ 0, "end choice",		ASN1_EOC,          ASN1_END				}, /* 15 */
	{ 0, "registeredID",	ASN1_CONTEXT_S_8,  ASN1_OPT|ASN1_BODY	}, /* 16 */
	{ 0, "end choice",		ASN1_EOC,          ASN1_END				}, /* 17 */
	{ 0, "exit",			ASN1_EOC,          ASN1_EXIT			}
};
#define GN_OBJ_OTHER_NAME	 	 0
#define GN_OBJ_RFC822_NAME	 	 2
#define GN_OBJ_DNS_NAME		 	 4
#define GN_OBJ_X400_ADDRESS	 	 6
#define GN_OBJ_DIRECTORY_NAME	 8
#define GN_OBJ_EDI_PARTY_NAME	10
#define GN_OBJ_URI				12
#define GN_OBJ_IP_ADDRESS		14
#define GN_OBJ_REGISTERED_ID	16

/**
 * Extracts a generalName
 */
static identification_t *parse_generalName(chunk_t blob, int level0)
{
	asn1_parser_t *parser;
	chunk_t object;
	int objectID ;
	
	identification_t *gn = NULL;
	
	parser = asn1_parser_create(generalNameObjects, blob);
	parser->set_top_level(parser, level0);
	
	while (parser->iterate(parser, &objectID, &object))
	{
		id_type_t id_type = ID_ANY;
		
		switch (objectID)
		{
			case GN_OBJ_RFC822_NAME:
				id_type = ID_RFC822_ADDR;
				break;
			case GN_OBJ_DNS_NAME:
				id_type = ID_FQDN;
				break;
			case GN_OBJ_URI:
				id_type = ID_DER_ASN1_GN_URI;
				break;
			case GN_OBJ_DIRECTORY_NAME:
				id_type = ID_DER_ASN1_DN;
				break;
			case GN_OBJ_IP_ADDRESS:
				id_type = ID_IPV4_ADDR;
				break;
			case GN_OBJ_OTHER_NAME:
				if (!parse_otherName(object, parser->get_level(parser)+1))
				{
					goto end;
				}
				break;
			case GN_OBJ_X400_ADDRESS:
			case GN_OBJ_EDI_PARTY_NAME:
			case GN_OBJ_REGISTERED_ID:
			default:
				break;
		}
		if (id_type != ID_ANY)
		{
			gn = identification_create_from_encoding(id_type, object);
			DBG2("  '%D'", gn);
			goto end;
		}
	}
	
end:
	parser->destroy(parser);
	return gn;
}

/**
 * ASN.1 definition of generalNames 
 */
static const asn1Object_t generalNamesObjects[] = {
	{ 0, "generalNames",	ASN1_SEQUENCE,	ASN1_LOOP }, /* 0 */
	{ 1,   "generalName",	ASN1_EOC,		ASN1_RAW  }, /* 1 */
	{ 0, "end loop",		ASN1_EOC,		ASN1_END  }, /* 2 */
	{ 0, "exit",			ASN1_EOC,		ASN1_EXIT }
};
#define GENERAL_NAMES_GN	1

/**
 * Extracts one or several GNs and puts them into a chained list
 */
void x509_parse_generalNames(chunk_t blob, int level0, bool implicit, linked_list_t *list)
{
	asn1_parser_t *parser;
	chunk_t object;
	int objectID;
	
	parser = asn1_parser_create(generalNamesObjects, blob);
	parser->set_top_level(parser, level0);
	parser->set_flags(parser, implicit, FALSE);
	
	while (parser->iterate(parser, &objectID, &object))
	{
		if (objectID == GENERAL_NAMES_GN)
		{
			identification_t *gn = parse_generalName(object,
											parser->get_level(parser)+1);
			
			if (gn)
			{
				list->insert_last(list, (void *)gn);
			}
		}
	}
	parser->destroy(parser);
}

/**
 * ASN.1 definition of a authorityKeyIdentifier extension 
 */
static const asn1Object_t authKeyIdentifierObjects[] = {
	{ 0, "authorityKeyIdentifier",		ASN1_SEQUENCE,		ASN1_NONE 			}, /* 0 */
	{ 1,   "keyIdentifier",				ASN1_CONTEXT_S_0,	ASN1_OPT|ASN1_BODY	}, /* 1 */
	{ 1,   "end opt",					ASN1_EOC,			ASN1_END  			}, /* 2 */
	{ 1,   "authorityCertIssuer",		ASN1_CONTEXT_C_1,	ASN1_OPT|ASN1_OBJ	}, /* 3 */
	{ 1,   "end opt",					ASN1_EOC,			ASN1_END  			}, /* 4 */
	{ 1,   "authorityCertSerialNumber",	ASN1_CONTEXT_S_2,	ASN1_OPT|ASN1_BODY	}, /* 5 */
	{ 1,   "end opt",					ASN1_EOC,			ASN1_END  			}, /* 6 */
	{ 0, "exit",						ASN1_EOC,			ASN1_EXIT  			}
};
#define AUTH_KEY_ID_KEY_ID			1
#define AUTH_KEY_ID_CERT_ISSUER		3
#define AUTH_KEY_ID_CERT_SERIAL		5

/**
 * Extracts an authoritykeyIdentifier
 */
identification_t* x509_parse_authorityKeyIdentifier(chunk_t blob, int level0,
												chunk_t *authKeySerialNumber)
{
	asn1_parser_t *parser;
	chunk_t object;
	int objectID;
	identification_t *authKeyIdentifier = NULL;
	
	*authKeySerialNumber = chunk_empty;
	
	parser = asn1_parser_create(authKeyIdentifierObjects, blob);
	parser->set_top_level(parser, level0);
	
	while (parser->iterate(parser, &objectID, &object))
	{
		switch (objectID) 
		{
			case AUTH_KEY_ID_KEY_ID:
				authKeyIdentifier = identification_create_from_encoding(
												ID_PUBKEY_SHA1, object); 
				break;
			case AUTH_KEY_ID_CERT_ISSUER:
				/* TODO: x509_parse_generalNames(object, level+1, TRUE); */
				break;
			case AUTH_KEY_ID_CERT_SERIAL:
				*authKeySerialNumber = object;
				break;
			default:
				break;
		}
	}
	parser->destroy(parser);
	return authKeyIdentifier;
}

/**
 * ASN.1 definition of a authorityInfoAccess extension 
 */
static const asn1Object_t authInfoAccessObjects[] = {
	{ 0, "authorityInfoAccess",	ASN1_SEQUENCE,	ASN1_LOOP }, /* 0 */
	{ 1,   "accessDescription",	ASN1_SEQUENCE,	ASN1_NONE }, /* 1 */
	{ 2,     "accessMethod",	ASN1_OID,		ASN1_BODY }, /* 2 */
	{ 2,     "accessLocation",	ASN1_EOC,		ASN1_RAW  }, /* 3 */
	{ 0, "end loop",			ASN1_EOC,		ASN1_END  }, /* 4 */
	{ 0, "exit",				ASN1_EOC,		ASN1_EXIT }
};
#define AUTH_INFO_ACCESS_METHOD		2
#define AUTH_INFO_ACCESS_LOCATION	3

/**
 * Extracts an authorityInfoAcess location
 */
static void parse_authorityInfoAccess(chunk_t blob, int level0,
									  private_x509_cert_t *this)
{
	asn1_parser_t *parser;
	chunk_t object;
	int objectID;
	int accessMethod = OID_UNKNOWN;
	
	parser = asn1_parser_create(authInfoAccessObjects, blob);
	parser->set_top_level(parser, level0);
	
	while (parser->iterate(parser, &objectID, &object))
	{
		switch (objectID) 
		{
			case AUTH_INFO_ACCESS_METHOD:
				accessMethod = asn1_known_oid(object);
				break;
			case AUTH_INFO_ACCESS_LOCATION:
			{
				switch (accessMethod)
				{
					case OID_OCSP:
					case OID_CA_ISSUERS:
						{
							identification_t *id;
							char *uri;
							
							id = parse_generalName(object,
											parser->get_level(parser)+1);
							if (id == NULL)
							{
								/* parsing went wrong - abort */
								goto end;
							}
							DBG2("  '%D'", id);
							if (accessMethod == OID_OCSP &&
								asprintf(&uri, "%D", id) > 0)
							{
								this->ocsp_uris->insert_last(this->ocsp_uris, uri);
							}
							id->destroy(id);
						}
						break;
					default:
						/* unkown accessMethod, ignoring */
						break;
				}
				break;
			}
			default:
				break;
		}
	}
	
end:
	parser->destroy(parser);
}

/**
 * ASN.1 definition of a extendedKeyUsage extension
 */
static const asn1Object_t extendedKeyUsageObjects[] = {
	{ 0, "extendedKeyUsage",	ASN1_SEQUENCE,	ASN1_LOOP }, /* 0 */
	{ 1,   "keyPurposeID",		ASN1_OID,		ASN1_BODY }, /* 1 */
	{ 0, "end loop",			ASN1_EOC,		ASN1_END  }, /* 2 */
	{ 0, "exit",				ASN1_EOC,		ASN1_EXIT }
};
#define EXT_KEY_USAGE_PURPOSE_ID	1

/**
 * Extracts extendedKeyUsage OIDs - currently only OCSP_SIGING is returned
 */
static bool parse_extendedKeyUsage(chunk_t blob, int level0)
{
	asn1_parser_t *parser;
	chunk_t object;
	int objectID;
	bool ocsp_signing = FALSE;
	
	parser = asn1_parser_create(extendedKeyUsageObjects, blob);
	parser->set_top_level(parser, level0);
	
	while (parser->iterate(parser, &objectID, &object))
	{
		if (objectID == EXT_KEY_USAGE_PURPOSE_ID && 
			asn1_known_oid(object) == OID_OCSP_SIGNING)
		{
			ocsp_signing = TRUE;
		}
	}
	parser->destroy(parser);
	return ocsp_signing;
}

/**
 * ASN.1 definition of crlDistributionPoints
 */
static const asn1Object_t crlDistributionPointsObjects[] = {
	{ 0, "crlDistributionPoints",	ASN1_SEQUENCE,		ASN1_LOOP			}, /*  0 */
	{ 1,   "DistributionPoint",		ASN1_SEQUENCE,		ASN1_NONE			}, /*  1 */
	{ 2,     "distributionPoint",	ASN1_CONTEXT_C_0,	ASN1_OPT|ASN1_LOOP	}, /*  2 */
	{ 3,       "fullName",			ASN1_CONTEXT_C_0,	ASN1_OPT|ASN1_OBJ	}, /*  3 */
	{ 3,       "end choice",		ASN1_EOC,			ASN1_END			}, /*  4 */
	{ 3,       "nameRelToCRLIssuer",ASN1_CONTEXT_C_1,	ASN1_OPT|ASN1_BODY	}, /*  5 */
	{ 3,       "end choice",		ASN1_EOC,			ASN1_END			}, /*  6 */
	{ 2,     "end opt",				ASN1_EOC,			ASN1_END			}, /*  7 */
	{ 2,     "reasons",				ASN1_CONTEXT_C_1,	ASN1_OPT|ASN1_BODY	}, /*  8 */
	{ 2,     "end opt",				ASN1_EOC,			ASN1_END			}, /*  9 */
	{ 2,     "crlIssuer",			ASN1_CONTEXT_C_2,	ASN1_OPT|ASN1_BODY	}, /* 10 */
	{ 2,     "end opt",				ASN1_EOC,			ASN1_END			}, /* 11 */
	{ 0, "end loop",				ASN1_EOC,			ASN1_END			}, /* 12 */
	{ 0, "exit",					ASN1_EOC,			ASN1_EXIT			}
};
#define CRL_DIST_POINTS_FULLNAME	 3

/**
 * Extracts one or several crlDistributionPoints into a list
 */
static void parse_crlDistributionPoints(chunk_t blob, int level0,
										private_x509_cert_t *this)
{
	asn1_parser_t *parser;
	chunk_t object;
	int objectID;
	linked_list_t *list = linked_list_create();
	
	parser = asn1_parser_create(crlDistributionPointsObjects, blob);
	parser->set_top_level(parser, level0);
	
	while (parser->iterate(parser, &objectID, &object))
	{
		if (objectID == CRL_DIST_POINTS_FULLNAME)
		{
			identification_t *id;
			
			/* append extracted generalNames to existing chained list */
			x509_parse_generalNames(object, parser->get_level(parser)+1,
									TRUE, list);
	
			while (list->remove_last(list, (void**)&id) == SUCCESS)
			{
				char *uri;
				
				if (asprintf(&uri, "%D", id) > 0)
				{
					this->crl_uris->insert_last(this->crl_uris, uri);
				}
				id->destroy(id);
			}
		}
	}
	parser->destroy(parser);
	list->destroy(list);
}

/**
 * ASN.1 definition of an X.509v3 x509_cert
 */
static const asn1Object_t certObjects[] = {
	{ 0, "x509",					ASN1_SEQUENCE,		ASN1_OBJ			}, /*  0 */
	{ 1,   "tbsCertificate",		ASN1_SEQUENCE,		ASN1_OBJ			}, /*  1 */
	{ 2,     "DEFAULT v1",			ASN1_CONTEXT_C_0,	ASN1_DEF			}, /*  2 */
	{ 3,       "version",			ASN1_INTEGER,		ASN1_BODY			}, /*  3 */
	{ 2,     "serialNumber",		ASN1_INTEGER,		ASN1_BODY			}, /*  4 */
	{ 2,     "signature",			ASN1_EOC,			ASN1_RAW			}, /*  5 */
	{ 2,     "issuer",				ASN1_SEQUENCE,		ASN1_OBJ			}, /*  6 */
	{ 2,     "validity",			ASN1_SEQUENCE,		ASN1_NONE			}, /*  7 */
	{ 3,       "notBefore",			ASN1_EOC,			ASN1_RAW			}, /*  8 */
	{ 3,       "notAfter",			ASN1_EOC,			ASN1_RAW			}, /*  9 */
	{ 2,     "subject",				ASN1_SEQUENCE,		ASN1_OBJ			}, /* 10 */
	{ 2,     "subjectPublicKeyInfo",ASN1_SEQUENCE,		ASN1_RAW			}, /* 11 */
	{ 2,     "issuerUniqueID",		ASN1_CONTEXT_C_1,	ASN1_OPT			}, /* 12 */
	{ 2,     "end opt",				ASN1_EOC,			ASN1_END			}, /* 13 */
	{ 2,     "subjectUniqueID",		ASN1_CONTEXT_C_2,	ASN1_OPT			}, /* 14 */
	{ 2,     "end opt",				ASN1_EOC,			ASN1_END			}, /* 15 */
	{ 2,     "optional extensions",	ASN1_CONTEXT_C_3,	ASN1_OPT			}, /* 16 */
	{ 3,       "extensions",		ASN1_SEQUENCE,		ASN1_LOOP			}, /* 17 */
	{ 4,         "extension",		ASN1_SEQUENCE,		ASN1_NONE			}, /* 18 */
	{ 5,           "extnID",		ASN1_OID,			ASN1_BODY			}, /* 19 */
	{ 5,           "critical",		ASN1_BOOLEAN,		ASN1_DEF|ASN1_BODY	}, /* 20 */
	{ 5,           "extnValue",		ASN1_OCTET_STRING,	ASN1_BODY			}, /* 21 */
	{ 3,       "end loop",			ASN1_EOC,			ASN1_END			}, /* 22 */
	{ 2,     "end opt",				ASN1_EOC,			ASN1_END			}, /* 23 */
	{ 1,   "signatureAlgorithm",	ASN1_EOC,			ASN1_RAW			}, /* 24 */
	{ 1,   "signatureValue",		ASN1_BIT_STRING,	ASN1_BODY			}, /* 25 */
	{ 0, "exit",					ASN1_EOC,			ASN1_EXIT			}
};
#define X509_OBJ_TBS_CERTIFICATE				 1
#define X509_OBJ_VERSION						 3
#define X509_OBJ_SERIAL_NUMBER					 4
#define X509_OBJ_SIG_ALG						 5
#define X509_OBJ_ISSUER 						 6
#define X509_OBJ_NOT_BEFORE						 8
#define X509_OBJ_NOT_AFTER						 9
#define X509_OBJ_SUBJECT						10
#define X509_OBJ_SUBJECT_PUBLIC_KEY_INFO		11
#define X509_OBJ_EXTN_ID						19
#define X509_OBJ_CRITICAL						20
#define X509_OBJ_EXTN_VALUE						21
#define X509_OBJ_ALGORITHM						24
#define X509_OBJ_SIGNATURE						25

/**
 * Parses an X.509v3 certificate
 */
static bool parse_certificate(private_x509_cert_t *this)
{
	asn1_parser_t *parser;
	chunk_t object;
	int objectID;
	int extn_oid = OID_UNKNOWN;
	int sig_alg  = OID_UNKNOWN;
	bool success = FALSE;
	bool critical;
	
	parser = asn1_parser_create(certObjects, this->encoding);
	
	while (parser->iterate(parser, &objectID, &object))
	{
		u_int level = parser->get_level(parser)+1;
		
		switch (objectID)
		{
			case X509_OBJ_TBS_CERTIFICATE:
				this->tbsCertificate = object;
				break;
			case X509_OBJ_VERSION:
				this->version = (object.len) ? (1+(u_int)*object.ptr) : 1;
				DBG2("  v%d", this->version);
				break;
			case X509_OBJ_SERIAL_NUMBER:
				this->serialNumber = object;
				break;
			case X509_OBJ_SIG_ALG:
				sig_alg = asn1_parse_algorithmIdentifier(object, level, NULL);
				break;
			case X509_OBJ_ISSUER:
				this->issuer = identification_create_from_encoding(ID_DER_ASN1_DN, object);
				DBG2("  '%D'", this->issuer);
				break;
			case X509_OBJ_NOT_BEFORE:
				this->notBefore = asn1_parse_time(object, level);
				break;
			case X509_OBJ_NOT_AFTER:
				this->notAfter = asn1_parse_time(object, level);
				break;
			case X509_OBJ_SUBJECT:
				this->subject = identification_create_from_encoding(ID_DER_ASN1_DN, object);
				DBG2("  '%D'", this->subject);
				break;
			case X509_OBJ_SUBJECT_PUBLIC_KEY_INFO:
				this->public_key = lib->creds->create(lib->creds, CRED_PUBLIC_KEY,
						KEY_ANY, BUILD_BLOB_ASN1_DER, object, BUILD_END);
				if (this->public_key == NULL)
				{
					DBG1("could not create public key");
					goto end;
				}
				break;
			case X509_OBJ_EXTN_ID:
				extn_oid = asn1_known_oid(object);
				break;
			case X509_OBJ_CRITICAL:
				critical = object.len && *object.ptr;
				DBG2("  %s", critical ? "TRUE" : "FALSE");
				break;
			case X509_OBJ_EXTN_VALUE:
			{
				switch (extn_oid)
				{
					case OID_SUBJECT_KEY_ID:
						if (!asn1_parse_simple_object(&object, ASN1_OCTET_STRING,
													  level, "keyIdentifier"))
						{
							goto end;
						}
						this->subjectKeyID = object;
						break;
					case OID_SUBJECT_ALT_NAME:
						x509_parse_generalNames(object, level, FALSE,
												this->subjectAltNames);
						break;
					case OID_BASIC_CONSTRAINTS:
						if (parse_basicConstraints(object, level))
						{
							this->flags |= X509_CA;
						}
						break;
					case OID_CRL_DISTRIBUTION_POINTS:
						parse_crlDistributionPoints(object, level, this);
						break;
					case OID_AUTHORITY_KEY_ID:
						this->authKeyIdentifier = x509_parse_authorityKeyIdentifier(object,
							 							level, &this->authKeySerialNumber);
						break;
					case OID_AUTHORITY_INFO_ACCESS:
						parse_authorityInfoAccess(object, level, this);
						break;
					case OID_EXTENDED_KEY_USAGE:
						if (parse_extendedKeyUsage(object, level))
						{
							this->flags |= X509_OCSP_SIGNER;
						}
						break;
					case OID_NS_REVOCATION_URL:
					case OID_NS_CA_REVOCATION_URL:
					case OID_NS_CA_POLICY_URL:
					case OID_NS_COMMENT:
						if (!asn1_parse_simple_object(&object, ASN1_IA5STRING, 
											level, oid_names[extn_oid].name))
						{
							goto end;
						}
						break;
					default:
						break;
				}
				break;
			}
			case X509_OBJ_ALGORITHM:
				this->algorithm = asn1_parse_algorithmIdentifier(object, level, NULL);
				if (this->algorithm != sig_alg)
				{
					DBG1("  signature algorithms do not agree");
					goto end;
				}
				break;
			case X509_OBJ_SIGNATURE:
				this->signature = object;
				break;
			default:
				break;
		}
	}
	success = parser->success(parser);
	
end:
	parser->destroy(parser);
	return success;
}

/**
 * Implementation of certificate_t.get_type
 */
static certificate_type_t get_type(private_x509_cert_t *this)
{
	return CERT_X509;
}

/**
 * Implementation of certificate_t.get_subject
 */
static identification_t* get_subject(private_x509_cert_t *this)
{
	return this->subject;
}

/**
 * Implementation of certificate_t.get_issuer
 */
static identification_t* get_issuer(private_x509_cert_t *this)
{
	return this->issuer;
}

/**
 * Implementation of certificate_t.has_subject.
 */
static id_match_t has_subject(private_x509_cert_t *this, identification_t *subject)
{
	identification_t *current;
	enumerator_t *enumerator;
	id_match_t match, best;
	
	if (this->encoding_hash.ptr && subject->get_type(subject) == ID_CERT_DER_SHA1 &&
		chunk_equals(this->encoding_hash, subject->get_encoding(subject)))
	{
		return ID_MATCH_PERFECT;
	}
	
	best = this->subject->matches(this->subject, subject);
	enumerator = this->subjectAltNames->create_enumerator(this->subjectAltNames);
	while (enumerator->enumerate(enumerator, &current))
	{
		match = current->matches(current, subject);
		if (match > best)
		{
			best = match;	
		}
	}
	enumerator->destroy(enumerator);
	return best;	
}

/**
 * Implementation of certificate_t.has_subject.
 */
static id_match_t has_issuer(private_x509_cert_t *this, identification_t *issuer)
{
	/* issuerAltNames currently not supported */
	return this->issuer->matches(this->issuer, issuer);
}

/**
 * Implementation of certificate_t.issued_by
 */
static bool issued_by(private_x509_cert_t *this, certificate_t *issuer)
{
	public_key_t *key;
	signature_scheme_t scheme;
	bool valid;
	x509_t *x509 = (x509_t*)issuer;
	
	if (&this->public.interface.interface == issuer)
	{
		if (this->flags & X509_SELF_SIGNED)
		{
			return TRUE;
		}
	}
	else
	{
		if (issuer->get_type(issuer) != CERT_X509)
		{
			return FALSE;
		}
		if (!(x509->get_flags(x509) & X509_CA))
		{
			return FALSE;
		}
	}
	if (!this->issuer->equals(this->issuer, issuer->get_subject(issuer)))
	{
		return FALSE;
	}
	/* TODO: generic OID to scheme mapper? */
	switch (this->algorithm)
	{
		case OID_MD5_WITH_RSA:
			scheme = SIGN_RSA_EMSA_PKCS1_MD5;
			break;
		case OID_SHA1_WITH_RSA:
			scheme = SIGN_RSA_EMSA_PKCS1_SHA1;
			break;
		case OID_SHA256_WITH_RSA:
			scheme = SIGN_RSA_EMSA_PKCS1_SHA256;
			break;
		case OID_SHA384_WITH_RSA:
			scheme = SIGN_RSA_EMSA_PKCS1_SHA384;
			break;
		case OID_SHA512_WITH_RSA:
			scheme = SIGN_RSA_EMSA_PKCS1_SHA512;
			break;
		case OID_ECDSA_WITH_SHA1:
			scheme = SIGN_ECDSA_WITH_SHA1;
			break;
		default:
			return FALSE;
	}
	key = issuer->get_public_key(issuer);
	if (key == NULL)
	{
		return FALSE;
	}
	/* TODO: add a lightweight check option (comparing auth/subject keyids only) */
	valid = key->verify(key, scheme, this->tbsCertificate, this->signature);
	key->destroy(key);
	return valid;
}

/**
 * Implementation of certificate_t.get_public_key
 */
static public_key_t* get_public_key(private_x509_cert_t *this)
{
	this->public_key->get_ref(this->public_key);
	return this->public_key;
}

/**
 * Implementation of certificate_t.asdf
 */
static private_x509_cert_t* get_ref(private_x509_cert_t *this)
{
	ref_get(&this->ref);
	return this;
}

/**
 * Implementation of x509_cert_t.get_flags.
 */
static x509_flag_t get_flags(private_x509_cert_t *this)
{
	return this->flags;
}

/**
 * Implementation of x509_cert_t.get_validity.
 */
static bool get_validity(private_x509_cert_t *this, time_t *when,
						 time_t *not_before, time_t *not_after)
{
	time_t t;
	
	if (when)
	{
		t = *when;
	}
	else
	{
		t = time(NULL);
	}
	if (not_before)
	{
		*not_before = this->notBefore;
	}
	if (not_after)
	{
		*not_after = this->notAfter;
	}
	return (t >= this->notBefore && t <= this->notAfter);
}

/**
 * Implementation of certificate_t.is_newer.
 */
static bool is_newer(certificate_t *this, certificate_t *that)
{
	time_t this_update, that_update, now = time(NULL);
	bool new;
	
	this->get_validity(this, &now, &this_update, NULL);
	that->get_validity(that, &now, &that_update, NULL);
	new = this_update > that_update;
	DBG1("  certificate from %#T is %s - existing certificate from %#T %s",
				&this_update, FALSE, new ? "newer":"not newer",
				&that_update, FALSE, new ? "replaced":"retained");
	return new;
}
	
/**
 * Implementation of certificate_t.get_encoding.
 */
static chunk_t get_encoding(private_x509_cert_t *this)
{
	return chunk_clone(this->encoding);
}

/**
 * Implementation of certificate_t.equals.
 */
static bool equals(private_x509_cert_t *this, certificate_t *other)
{
	chunk_t encoding;
	bool equal;
	
	if (this == (private_x509_cert_t*)other)
	{
		return TRUE;
	}
	if (other->get_type(other) != CERT_X509)
	{
		return FALSE;
	}
	if (other->equals == (void*)equals)
	{	/* skip allocation if we have the same implementation */
		return chunk_equals(this->encoding, ((private_x509_cert_t*)other)->encoding); 
	}
	encoding = other->get_encoding(other);
	equal = chunk_equals(this->encoding, encoding);
	free(encoding.ptr);
	return equal;
}

/**
 * Implementation of x509_t.get_serial.
 */
static chunk_t get_serial(private_x509_cert_t *this)
{
	return this->serialNumber;
}

/**
 * Implementation of x509_t.get_authKeyIdentifier.
 */
static identification_t *get_authKeyIdentifier(private_x509_cert_t *this)
{
	return this->authKeyIdentifier;
}

/**
 * Implementation of x509_cert_t.create_subjectAltName_enumerator.
 */
static enumerator_t* create_subjectAltName_enumerator(private_x509_cert_t *this)
{
	return this->subjectAltNames->create_enumerator(this->subjectAltNames);
}

/**
 * Implementation of x509_cert_t.create_ocsp_uri_enumerator.
 */
static enumerator_t* create_ocsp_uri_enumerator(private_x509_cert_t *this)
{
	return this->ocsp_uris->create_enumerator(this->ocsp_uris);
}

/**
 * Implementation of x509_cert_t.create_crl_uri_enumerator.
 */
static enumerator_t* create_crl_uri_enumerator(private_x509_cert_t *this)
{
	return this->crl_uris->create_enumerator(this->crl_uris);
}

/**
 * Implementation of certificate_t.asdf
 */
static void destroy(private_x509_cert_t *this)
{
	if (ref_put(&this->ref))
	{
		this->subjectAltNames->destroy_offset(this->subjectAltNames,
									offsetof(identification_t, destroy));
		this->crl_uris->destroy_function(this->crl_uris, free);
		this->ocsp_uris->destroy_function(this->ocsp_uris, free);
		DESTROY_IF(this->issuer);
		DESTROY_IF(this->subject);
		DESTROY_IF(this->public_key);
		DESTROY_IF(this->authKeyIdentifier);
		chunk_free(&this->encoding);
		chunk_free(&this->encoding_hash);
		if (!this->parsed)
		{	/* only parsed certificates point these fields to "encoded" */
			chunk_free(&this->signature);
			chunk_free(&this->serialNumber);
			chunk_free(&this->tbsCertificate);
		}
		free(this);
	}
}

/**
 * create an empty but initialized X.509 certificate
 */
static private_x509_cert_t* create_empty(void)
{
	private_x509_cert_t *this = malloc_thing(private_x509_cert_t);
	
	this->public.interface.interface.get_type = (certificate_type_t (*)(certificate_t *this))get_type;
	this->public.interface.interface.get_subject = (identification_t* (*)(certificate_t *this))get_subject;
	this->public.interface.interface.get_issuer = (identification_t* (*)(certificate_t *this))get_issuer;
	this->public.interface.interface.has_subject = (id_match_t (*)(certificate_t*, identification_t *subject))has_subject;
	this->public.interface.interface.has_issuer = (id_match_t (*)(certificate_t*, identification_t *issuer))has_issuer;
	this->public.interface.interface.issued_by = (bool (*)(certificate_t *this, certificate_t *issuer))issued_by;
	this->public.interface.interface.get_public_key = (public_key_t* (*)(certificate_t *this))get_public_key;
	this->public.interface.interface.get_validity = (bool (*)(certificate_t*, time_t *when, time_t *, time_t*))get_validity;
	this->public.interface.interface.is_newer = (bool (*)(certificate_t*,certificate_t*))is_newer;
	this->public.interface.interface.get_encoding = (chunk_t (*)(certificate_t*))get_encoding;
	this->public.interface.interface.equals = (bool (*)(certificate_t*, certificate_t *other))equals;
	this->public.interface.interface.get_ref = (certificate_t* (*)(certificate_t *this))get_ref;
	this->public.interface.interface.destroy = (void (*)(certificate_t *this))destroy;
	this->public.interface.get_flags = (x509_flag_t (*)(x509_t*))get_flags;
	this->public.interface.get_serial = (chunk_t (*)(x509_t*))get_serial;
	this->public.interface.get_authKeyIdentifier = (identification_t* (*)(x509_t*))get_authKeyIdentifier;
	this->public.interface.create_subjectAltName_enumerator = (enumerator_t* (*)(x509_t*))create_subjectAltName_enumerator;
	this->public.interface.create_crl_uri_enumerator = (enumerator_t* (*)(x509_t*))create_crl_uri_enumerator;
	this->public.interface.create_ocsp_uri_enumerator = (enumerator_t* (*)(x509_t*))create_ocsp_uri_enumerator;
	
	this->encoding = chunk_empty;
	this->encoding_hash = chunk_empty;
	this->tbsCertificate = chunk_empty;
	this->version = 3;
	this->serialNumber = chunk_empty;	
	this->notBefore = 0;
	this->notAfter = 0;
	this->public_key = NULL;
	this->subject = NULL;
	this->issuer = NULL;
	this->subjectAltNames = linked_list_create();
	this->crl_uris = linked_list_create();
	this->ocsp_uris = linked_list_create();
	this->subjectKeyID = chunk_empty;
	this->authKeyIdentifier = NULL;
	this->authKeySerialNumber = chunk_empty;
	this->algorithm = 0;
	this->signature = chunk_empty;
	this->flags = 0;
	this->ref = 1;
	this->parsed = FALSE;
	
	return this;
}

/**
 * create an X.509 certificate from a chunk
 */
static private_x509_cert_t *create_from_chunk(chunk_t chunk)
{
	hasher_t *hasher;
	private_x509_cert_t *this = create_empty();
	
	this->encoding = chunk;
	if (!parse_certificate(this))
	{
		destroy(this);
		return NULL;
	}
	
	/* check if the certificate is self-signed */
	if (issued_by(this, &this->public.interface.interface))
	{
		this->flags |= X509_SELF_SIGNED;
	}
	
	hasher = lib->crypto->create_hasher(lib->crypto, HASH_SHA1);
	if (hasher != NULL)
	{
		hasher->allocate_hash(hasher, this->encoding, &this->encoding_hash);
		hasher->destroy(hasher);
	}
	else
	{
		DBG1("  unable to create hash of certificate, SHA1 not supported");		
	}
	
	this->parsed = TRUE;
	return this;
}

/**
 * create an X.509 certificate from a file
 */
static private_x509_cert_t *create_from_file(char *path)
{
	bool pgp = FALSE;
	chunk_t chunk;
	private_x509_cert_t *this;
	
	if (!pem_asn1_load_file(path, NULL, &chunk, &pgp))
	{
		return NULL;
	}

	this = create_from_chunk(chunk);

	if (this == NULL)
	{
		DBG1("  could not parse loaded certificate file '%s'",path);
		return NULL;
	}
	DBG1("  loaded certificate file '%s'",  path);
	return this;
}

typedef struct private_builder_t private_builder_t;
/**
 * Builder implementation for certificate loading
 */
struct private_builder_t {
	/** implements the builder interface */
	builder_t public;
	/** loaded certificate */
	private_x509_cert_t *cert;
	/** additional flags to enforce */
	x509_flag_t flags;
	/** certificate to sign, if we generate a new cert */
	certificate_t *sign_cert;
	/** private key to sign, if we generate a new cert */
	private_key_t *sign_key;
};

/**
 * Generate and sign a new certificate
 */
static bool generate(private_builder_t *this)
{
	chunk_t extensions = chunk_empty;
	identification_t *issuer, *subject;
	chunk_t key_info, key;
	signature_scheme_t scheme;
	hasher_t *hasher;
	
	subject = this->cert->subject;
	if (this->sign_cert)
	{
		issuer = this->sign_cert->get_subject(this->sign_cert);
		if (!this->cert->public_key)
		{
			return FALSE;
		}
	}
	else
	{	/* self signed */
		issuer = subject;
		if (!this->cert->public_key)
		{
			this->cert->public_key = this->sign_key->get_public_key(this->sign_key);
		}
		this->flags |= X509_SELF_SIGNED;
	}
	this->cert->issuer = issuer->clone(issuer);
	if (!this->cert->notBefore)
	{
		this->cert->notBefore = time(NULL);
	}
	if (!this->cert->notAfter)
	{	/* defaults to 1 years from now on */
		this->cert->notAfter = this->cert->notBefore + 60 * 60 * 24 * 365;
	}
	this->cert->flags = this->flags;
	
	switch (this->sign_key->get_type(this->sign_key))
	{
		case KEY_RSA:
			this->cert->algorithm = OID_SHA1_WITH_RSA;
			scheme = SIGN_RSA_EMSA_PKCS1_SHA1;
			break;
		default:
			return FALSE;
	}
	
	switch (this->cert->public_key->get_type(this->cert->public_key))
	{
		case KEY_RSA:
			key = this->cert->public_key->get_encoding(this->cert->public_key);
			key_info = asn1_wrap(ASN1_SEQUENCE, "cm",
							asn1_algorithmIdentifier(OID_RSA_ENCRYPTION), 
							asn1_bitstring("m", key));	
			break;
		default:
			return FALSE;
	}
	
	if (this->cert->subjectAltNames->get_count(this->cert->subjectAltNames))
	{
		/* TODO: encode subjectAltNames */
	}
	
	this->cert->tbsCertificate = asn1_wrap(ASN1_SEQUENCE, "mmccmcmm", 
		asn1_simple_object(ASN1_CONTEXT_C_0, ASN1_INTEGER_2),
		asn1_simple_object(ASN1_INTEGER, this->cert->serialNumber),
		asn1_algorithmIdentifier(this->cert->algorithm),
		issuer->get_encoding(issuer),
		asn1_wrap(ASN1_SEQUENCE, "mm",
			asn1_from_time(&this->cert->notBefore, ASN1_UTCTIME),
			asn1_from_time(&this->cert->notAfter, ASN1_UTCTIME)),
		subject->get_encoding(subject),
		key_info, extensions);
	
	if (!this->sign_key->sign(this->sign_key, scheme, 
							this->cert->tbsCertificate, &this->cert->signature))
	{
		return FALSE;
	}
	this->cert->encoding = asn1_wrap(ASN1_SEQUENCE, "ccm",
								this->cert->tbsCertificate,
								asn1_algorithmIdentifier(this->cert->algorithm),
								asn1_bitstring("c", this->cert->signature));
	
	hasher = lib->crypto->create_hasher(lib->crypto, HASH_SHA1);
	if (!hasher)
	{
		return FALSE;
	}
	hasher->allocate_hash(hasher, this->cert->encoding,
						  &this->cert->encoding_hash);
	hasher->destroy(hasher);
	return TRUE;
}

/**
 * Implementation of builder_t.build
 */
static private_x509_cert_t *build(private_builder_t *this)
{
	private_x509_cert_t *cert;
	x509_flag_t flags;
	
	if (this->cert && !this->cert->encoding.ptr)
	{
		if (!this->sign_key || !this->cert ||
			!generate(this))
		{
			destroy(this->cert);
			free(this);
			return NULL;
		}
	}
	cert = this->cert;
	flags =  this->flags;
	free(this);
	if (cert == NULL)
	{
		return NULL;
	}
	
	if ((flags & X509_CA) && !(cert->flags & X509_CA))
	{
		DBG1("  ca certificate must have ca basic constraint set, discarded");
		destroy(cert);
		return NULL;
	}
	cert->flags |= flags;
	return cert;
}

/**
 * Implementation of builder_t.add
 */
static void add(private_builder_t *this, builder_part_t part, ...)
{
	va_list args;
	chunk_t chunk;
	bool handled = TRUE;
	
	va_start(args, part);
	switch (part)
	{
		case BUILD_FROM_FILE:
			this->cert = create_from_file(va_arg(args, char*));
			break;
		case BUILD_BLOB_ASN1_DER:
			chunk = va_arg(args, chunk_t);
			this->cert = create_from_chunk(chunk_clone(chunk));
			break;
		case BUILD_X509_FLAG:
			this->flags = va_arg(args, x509_flag_t);
			break;
		case BUILD_SIGNING_KEY:
			this->sign_key = va_arg(args, private_key_t*);
			break;
		case BUILD_SIGNING_CERT:
			this->sign_cert = va_arg(args, certificate_t*);
			break;
		default:
			/* all other parts need an empty cert */
			if (!this->cert)
			{
				this->cert = create_empty();
			}
			handled = FALSE;
			break;
	}
	if (handled)
	{
		va_end(args);
		return;
	}
	
	switch (part)
	{
		case BUILD_PUBLIC_KEY:
		{
			public_key_t *key = va_arg(args, public_key_t*);
			this->cert->public_key = key->get_ref(key);
			break;
		}
		case BUILD_SUBJECT:
		{
			identification_t *id = va_arg(args, identification_t*);
			this->cert->subject = id->clone(id);
			break;
		}
		case BUILD_SUBJECT_ALTNAME:
		{
			identification_t *id = va_arg(args, identification_t*);
			this->cert->subjectAltNames->insert_last(
									this->cert->subjectAltNames, id->clone(id));
			break;
		}
		case BUILD_NOT_BEFORE_TIME:
			this->cert->notBefore = va_arg(args, time_t);
			break;
		case BUILD_NOT_AFTER_TIME:
			this->cert->notAfter = va_arg(args, time_t);
			break;
		case BUILD_SERIAL:
		{
			chunk_t serial = va_arg(args, chunk_t);
			this->cert->serialNumber = chunk_clone(serial);
			break;
		}
		default:
			/* abort if unsupported option */
			if (this->cert)
			{
				destroy(this->cert);
			}
			builder_cancel(&this->public);
			break;
	}
	va_end(args);
}

/**
 * Builder construction function
 */
builder_t *x509_cert_builder(certificate_type_t type)
{
	private_builder_t *this;
	
	if (type != CERT_X509)
	{
		return NULL;
	}
	
	this = malloc_thing(private_builder_t);
	
	this->cert = NULL;
	this->flags = 0;
	this->sign_cert = NULL;
	this->sign_key = NULL;
	this->public.add = (void(*)(builder_t *this, builder_part_t part, ...))add;
	this->public.build = (void*(*)(builder_t *this))build;
	
	return &this->public;
}

