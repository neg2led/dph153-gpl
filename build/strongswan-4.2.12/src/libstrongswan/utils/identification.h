/*
 * Copyright (C) 2005-2006 Martin Willi
 * Copyright (C) 2005 Jan Hutter
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
 * $Id: identification.h 4344 2008-09-17 02:17:01Z andreas $
 */
 
/**
 * @defgroup identification identification
 * @{ @ingroup utils
 */


#ifndef IDENTIFICATION_H_
#define IDENTIFICATION_H_

typedef enum id_type_t id_type_t;
typedef struct identification_t identification_t;
typedef enum id_match_t id_match_t;

#include <library.h>

/** 
 * Matches returned from identification_t.match
 */
enum id_match_t {
	/* no match */
	ID_MATCH_NONE = 0,
	/* match to %any ID */
	ID_MATCH_ANY = 1,
	/* match with maximum allowed wildcards */
	ID_MATCH_MAX_WILDCARDS = 2,
	/* match with only one wildcard */
	ID_MATCH_ONE_WILDCARD = 19,
	/* perfect match, won't get better */
	ID_MATCH_PERFECT = 20,
};

/**
 * enum names for id_match_t.
 */
extern enum_name_t *id_match_names;

/**
 * ID Types in a ID payload.
 */
enum id_type_t {

	/**
	 * private type which matches any other id.
	 */
	ID_ANY = 0,

	/**
	 * ID data is a single four (4) octet IPv4 address.
	 */
	ID_IPV4_ADDR = 1,

	/**
	 * ID data is a fully-qualified domain name string.
	 * An example of a ID_FQDN is "example.com".
	 * The string MUST not contain any terminators (e.g., NULL, CR, etc.).
	 */
	ID_FQDN = 2,

	/**
	 * ID data is a fully-qualified RFC822 email address string.
	 * An example of an ID_RFC822_ADDR is "jsmith@example.com".
	 * The string MUST NOT contain any terminators.
	 */
	ID_RFC822_ADDR = 3,

	/**
	 * ID data is an IPv4 subnet (IKEv1 only)
	 */
	ID_IPV4_ADDR_SUBNET = 4,

	/**
	 * ID data is a single sixteen (16) octet IPv6 address.
	 */
	ID_IPV6_ADDR = 5,

	/**
	 * ID data is an IPv6 subnet (IKEv1 only)
	 */
	ID_IPV6_ADDR_SUBNET = 6,

	/**
	 * ID data is an IPv4 address range (IKEv1 only)
	 */
	ID_IPV4_ADDR_RANGE = 7,

	/**
	 * ID data is an IPv6 address range (IKEv1 only)
	 */
	ID_IPV6_ADDR_RANGE = 8,

	/**
	 * ID data is the binary DER encoding of an ASN.1 X.501 Distinguished Name
	 */
	ID_DER_ASN1_DN = 9,

	/**
	 * ID data is the binary DER encoding of an ASN.1 X.509 GeneralName
	 */
	ID_DER_ASN1_GN = 10,

	/**
	 * ID data is an opaque octet stream which may be used to pass vendor-
	 * specific information necessary to do certain proprietary
	 * types of identification.
	 */
	ID_KEY_ID = 11,

	/**
	 * private type which represents a GeneralName of type URI
	 */
	ID_DER_ASN1_GN_URI = 201,
	
	/**
	 * SHA1 hash over PKCS#1 subjectPublicKeyInfo
	 */
	ID_PUBKEY_INFO_SHA1 = 202,
	
	/**
	 * SHA1 hash over PKCS#1 subjectPublicKey
	 */
	ID_PUBKEY_SHA1 = 203,
	
	/**
	 * SHA1 hash of the binary DER encoding of a certificate
	 */
	ID_CERT_DER_SHA1 = 204,
	
	/**
	 * Generic EAP identity
	 */
	ID_EAP = 205,

	/**
	 * IETF Attribute Syntax String (RFC 3281)
	 */
	ID_IETF_ATTR_STRING = 206,
};

/**
 * enum names for id_type_t.
 */
extern enum_name_t *id_type_names;

/**
 * Generic identification, such as used in ID payload.
 * 
 * @todo Support for ID_DER_ASN1_GN is minimal right now. Comparison
 * between them and ID_IPV4_ADDR/RFC822_ADDR would be nice.
 */
struct identification_t {
	
	/**
	 * Get the encoding of this id, to send over
	 * the network.
	 * 
	 * Result points to internal data, do not free.
	 * 
	 * @return 			a chunk containing the encoded bytes
	 */
	chunk_t (*get_encoding) (identification_t *this);
	
	/**
	 * Get the type of this identification.
	 * 
	 * @return 			id_type_t
	 */
	id_type_t (*get_type) (identification_t *this);
	
	/**
	 * Check if two identification_t objects are equal.
	 * 
	 * @param other		other identification_t object
	 * @return 			TRUE if the IDs are equal
	 */
	bool (*equals) (identification_t *this, identification_t *other);
	
	/**
	 * Check if an ID matches a wildcard ID.
	 * 
	 * An identification_t may contain wildcards, such as
	 * *@strongswan.org. This call checks if a given ID
	 * (e.g. tester@strongswan.org) belongs to a such wildcard
	 * ID. Returns > 0 if
	 * - IDs are identical
	 * - other is of type ID_ANY
	 * - other contains a wildcard and matches this
	 *
	 * The larger the return value is, the better is the match. Zero means
	 * no match at all, 1 means a bad match, and 2 a slightly better match.
	 * 
	 * @param other		the ID containing one or more wildcards
	 * @param wildcards	returns the number of wildcards, may be NULL
	 * @return 			match value as described above
	 */
	id_match_t (*matches) (identification_t *this, identification_t *other);
	
	/**
	 * Check if an ID is a wildcard ID.
	 *
	 * If the ID represents multiple IDs (with wildcards, or
	 * as the type ID_ANY), TRUE is returned. If it is unique,
	 * FALSE is returned.
	 * 
	 * @return 			TRUE if ID contains wildcards
	 */
	bool (*contains_wildcards) (identification_t *this);
	
	/**
	 * Clone a identification_t instance.
	 * 
	 * @return 			clone of this
	 */
	identification_t *(*clone) (identification_t *this);

	/**
	 * Destroys a identification_t object.
	 */
	void (*destroy) (identification_t *this);
};

/**
 * Creates an identification_t object from a string.
 *
 * The input string may be e.g. one of the following:
 * - ID_IPV4_ADDR:		192.168.0.1
 * - ID_IPV6_ADDR:		2001:0db8:85a3:08d3:1319:8a2e:0370:7345
 * - ID_FQDN:			@www.strongswan.org (@indicates FQDN)
 * - ID_RFC822_ADDR:	alice@wonderland.org
 * - ID_DER_ASN1_DN:	C=CH, O=Linux strongSwan, CN=bob
 *
 * In favour of pluto, domainnames are prepended with an @, since
 * pluto resolves domainnames without an @ to IPv4 addresses. Since
 * we use a seperate host_t class for addresses, this doesn't
 * make sense for us.
 * 
 * A distinguished name may contain one or more of the following RDNs:
 * ND, UID, DC, CN, S, SN, serialNumber, C, L, ST, O, OU, T, D,
 * N, G, I, ID, EN, EmployeeNumber, E, Email, emailAddress, UN, 
 * unstructuredName, TCGID.
 * 
 * @param string	input string, which will be converted
 * @return			created identification_t, NULL if not supported.
 */
identification_t * identification_create_from_string(char *string);

/**
 * Creates an identification_t object from an encoded chunk.
 *
 * In contrast to identification_create_from_string(), this constructor never
 * returns NULL, even when the conversion to a string representation fails.
 * 
 * @param type		type of this id, such as ID_IPV4_ADDR
 * @param encoded	encoded bytes, such as from identification_t.get_encoding
 * @return			identification_t
 */
identification_t * identification_create_from_encoding(id_type_t type, chunk_t encoded);

/**
 * Get the printf hook functions.
 * 
 * @return			printf hook functions
 */
printf_hook_functions_t identification_get_printf_hooks();

#endif /* IDENTIFICATION_H_ @} */
