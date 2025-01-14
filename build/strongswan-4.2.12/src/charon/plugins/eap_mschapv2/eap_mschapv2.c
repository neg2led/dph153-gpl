/*
 * Copyright (C) 2009 Tobias Brunner
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
 * $Id: eap_mschapv2.c 4888 2009-02-19 14:32:13Z tobias $
 */

#include "eap_mschapv2.h"

#include <daemon.h>
#include <library.h>
#include <utils/enumerator.h>
#include <crypto/crypters/crypter.h>
#include <crypto/hashers/hasher.h>

typedef struct private_eap_mschapv2_t private_eap_mschapv2_t;

/**
 * Private data of an eap_mschapv2_t object.
 */
struct private_eap_mschapv2_t
{
	/**
	 * Public authenticator_t interface.
	 */
	eap_mschapv2_t public;
	
	/**
	 * ID of the server
	 */
	identification_t *server;
	
	/**
	 * ID of the peer
	 */
	identification_t *peer;
	
	/**
	 * challenge sent by the server
	 */
	chunk_t challenge;
	
	/**
	 * generated NT-Response
	 */
	chunk_t nt_response;
	
	/**
	 * generated Authenticator Response
	 */
	chunk_t auth_response;
	
	/**
	 * generated MSK
	 */
	chunk_t msk;
	
	/**
	 * EAP message identifier
	 */
	u_int8_t identifier;
	
	/**
	 * MS-CHAPv2-ID (session ID, increases with each retry)
	 */
	u_int8_t mschapv2id;
	
	/**
	 * Number of retries
	 */
	int retries;
};

/**
 * OpCodes
 */
enum mschapv2_opcode_t
{
	MSCHAPV2_CHALLENGE = 1,
	MSCHAPV2_RESPONSE = 2,
	MSCHAPV2_SUCCESS = 3,
	MSCHAPV2_FAILURE = 4,
	MSCHAPV2_CHANGE_PASSWORD = 7,
};

/**
 * Names for OpCodes
 */
ENUM_BEGIN(mschapv2_opcode_names, MSCHAPV2_CHALLENGE, MSCHAPV2_FAILURE,
		"CHALLENGE",
		"RESPONSE",
		"SUCCESS",
		"FAILURE");
ENUM_NEXT(mschapv2_opcode_names, MSCHAPV2_CHANGE_PASSWORD, MSCHAPV2_CHANGE_PASSWORD, MSCHAPV2_FAILURE,
		"CHANGE_PASSWORD");
ENUM_END(mschapv2_opcode_names, MSCHAPV2_CHANGE_PASSWORD);

/**
 * Error codes
 */
enum mschapv2_error_t
{
	ERROR_RESTRICTED_LOGON_HOURS	= 646,
	ERROR_ACCT_DISABLED				= 647,
	ERROR_PASSWD_EXPIRED			= 648,
	ERROR_NO_DIALIN_PERMISSION		= 649,
	ERROR_AUTHENTICATION_FAILURE	= 691,
	ERROR_CHANGING_PASSWORD			= 709,
};

/**
 * Names for error codes
 */
ENUM_BEGIN(mschapv2_error_names, ERROR_RESTRICTED_LOGON_HOURS, ERROR_NO_DIALIN_PERMISSION,
		"ERROR_RESTRICTED_LOGON_HOURS",
		"ERROR_ACCT_DISABLED",
		"ERROR_PASSWD_EXPIRED",
		"ERROR_NO_DIALIN_PERMISSION");
ENUM_NEXT(mschapv2_error_names, ERROR_AUTHENTICATION_FAILURE, ERROR_AUTHENTICATION_FAILURE, ERROR_NO_DIALIN_PERMISSION,
		"ERROR_AUTHENTICATION_FAILURE");
ENUM_NEXT(mschapv2_error_names, ERROR_CHANGING_PASSWORD, ERROR_CHANGING_PASSWORD, ERROR_AUTHENTICATION_FAILURE,
		"ERROR_CHANGING_PASSWORD");
ENUM_END(mschapv2_error_names, ERROR_CHANGING_PASSWORD);

/* Length of the challenge */
#define CHALLENGE_LEN 16
/* Length of the response (see eap_mschapv2_response_t) */
#define RESPONSE_LEN 49
/* Length of the authenticator response string ("S=<...>") */
#define AUTH_RESPONSE_LEN 42
/* Name we send as authenticator */
#define MSCHAPV2_HOST_NAME "strongSwan"
/* Message sent on success */
#define SUCCESS_MESSAGE " M=Welcome to strongSwan"
/* Message sent on failure */
#define FAILURE_MESSAGE "E=691 R=1 C="
/* Length of the complete failure message */
#define FAILURE_MESSAGE_LEN (sizeof(FAILURE_MESSAGE) + CHALLENGE_LEN * 2)

/* Number of seconds to delay retries */
#define RETRY_DELAY 2
/* Maximum number of retries */
#define MAX_RETRIES 2

typedef struct eap_mschapv2_header_t eap_mschapv2_header_t;
typedef struct eap_mschapv2_challenge_t eap_mschapv2_challenge_t;
typedef struct eap_mschapv2_response_t eap_mschapv2_response_t;

/**
 * packed EAP-MS-CHAPv2 header struct
 */
struct eap_mschapv2_header_t
{
	/** EAP code (REQUEST/RESPONSE) */
	u_int8_t code;
	/** unique message identifier */
	u_int8_t identifier;
	/** length of whole message */
	u_int16_t length;
	/** EAP type */
	u_int8_t type;
	/** MS-CHAPv2 OpCode */
	u_int8_t opcode;
	/** MS-CHAPv2-ID (equals identifier) */
	u_int8_t ms_chapv2_id;
	/** MS-Length (defined as length - 5) */
	u_int16_t ms_length;
	/** packet data (determined by OpCode) */
	u_int8_t data[];
}__attribute__((__packed__));

/**
 * packed data for a MS-CHAPv2 Challenge packet
 */
struct eap_mschapv2_challenge_t
{
	/** Value-Size */
	u_int8_t value_size;
	/** Challenge */
	u_int8_t challenge[CHALLENGE_LEN];
	/** Name */
	u_int8_t name[];
}__attribute__((__packed__));

/**
 * packed data for a MS-CHAPv2 Response packet
 */
struct eap_mschapv2_response_t
{
	/** Value-Size */
	u_int8_t value_size;
	/** Response */
	struct
	{
		/* Peer-Challenge*/
		u_int8_t peer_challenge[CHALLENGE_LEN];
		/* Reserved (=zero) */
		u_int8_t peer_reserved[8];
		/* NT-Response */
		u_int8_t nt_response[24];
		/* Flags (=zero) */
		u_int8_t flags;
	} response;
	/** Name */
	u_int8_t name[];
}__attribute__((__packed__));

/**
 * Length of the MS-CHAPv2 header
 */
#define HEADER_LEN (sizeof(eap_mschapv2_header_t))

/**
 * Length of the header for MS-CHAPv2 success/failure packets (does not include
 * MS-CHAPv2-ID and MS-Length, i.e. 3 octets)
 */
#define SHORT_HEADER_LEN (HEADER_LEN - 3)

/**
 * The minimum length of an MS-CHAPv2 Challenge packet (the name MUST be
 * at least one octet)
 */
#define CHALLENGE_PAYLOAD_LEN (HEADER_LEN + sizeof(eap_mschapv2_challenge_t))

/**
 * The minimum length of an MS-CHAPv2 Response packet
 */
#define RESPONSE_PAYLOAD_LEN (HEADER_LEN + sizeof(eap_mschapv2_response_t))


/**
 * Expand a 56-bit key to a 64-bit DES key by adding parity bits (odd parity)
 */
static chunk_t ExpandDESKey(chunk_t key)
{
	static const u_char bitmask[] =	{ 0xfe, 0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x80 };
	int i;
	u_char carry = 0;
	chunk_t expanded;
	
	/* expand the 7 octets to 8 octets */
	expanded = chunk_alloc(8);
	for (i = 0; i < 7; i++)
	{
		expanded.ptr[i] = ((key.ptr[i] & bitmask[i]) >> i) | (carry << (8 - i));
		carry = key.ptr[i] & ~bitmask[i];
	}
	expanded.ptr[7] = carry << 1;
	
	/* add parity bits to each octet */
	for (i = 0; i < 8; i++)
	{
		u_char val = expanded.ptr[i];
		val = (val ^ (val >> 4)) & 0x0f;
		expanded.ptr[i] |= (0x9669 >> val) & 1;
	}
	return expanded;
}

/**
 * Calculate the NT password hash (i.e. hash the (unicode) password with MD4) 
 */
static status_t NtPasswordHash(chunk_t password, chunk_t *password_hash)
{
	hasher_t *hasher;
	hasher = lib->crypto->create_hasher(lib->crypto, HASH_MD4);
	if (hasher == NULL)
	{
		DBG1(DBG_IKE, "EAP-MS-CHAPv2 failed, MD4 not supported");
		return FAILED;
	}
	hasher->allocate_hash(hasher, password, password_hash);
	hasher->destroy(hasher);
	return SUCCESS;
}

/**
 * Calculate the challenge hash (i.e. hash [peer_challenge | server_challenge |
 * username (without domain part)] with SHA1) 
 */
static status_t ChallengeHash(chunk_t peer_challenge, chunk_t server_challenge,
							  chunk_t username, chunk_t *challenge_hash)
{
	chunk_t concat;
	hasher_t *hasher;
	hasher = lib->crypto->create_hasher(lib->crypto, HASH_SHA1);
	if (hasher == NULL)
	{
		DBG1(DBG_IKE, "EAP-MS-CHAPv2 failed, SHA1 not supported");
		return FAILED;
	}
	concat = chunk_cata("ccc", peer_challenge, server_challenge, username);
	hasher->allocate_hash(hasher, concat, challenge_hash);
	hasher->destroy(hasher);
	/* we need only the first 8 octets */
	challenge_hash->len = 8;
	return SUCCESS;
}

/**
 * Calculate the challenge response (i.e. expand password_hash to three DES keys
 * and then encrypt the 8-octet challenge_hash with these keys and concatenate
 * the results).
 */
static status_t ChallengeResponse(chunk_t challenge_hash, chunk_t password_hash,
								  chunk_t *response)
{
	int i;
	crypter_t *crypter;
	chunk_t keys[3], z_password_hash;
	crypter = lib->crypto->create_crypter(lib->crypto, ENCR_DES_ECB, 8);
	if (crypter == NULL)
	{
		DBG1(DBG_IKE, "EAP-MS-CHAPv2 failed, DES-ECB not supported");
		return FAILED;
	}
	/* prepare keys: first pad password_hash to 21 octets, these get then split
	 * into 7-octet chunks, which then get expanded into 8-octet DES keys */
	z_password_hash = chunk_alloca(21);
	memset(z_password_hash.ptr, 0, z_password_hash.len);
	memcpy(z_password_hash.ptr, password_hash.ptr, password_hash.len);
	chunk_split(z_password_hash, "mmm", 7, &keys[0], 7, &keys[1], 7, &keys[2]);
	
	*response = chunk_alloc(24);
	for (i = 0; i < 3; i++)
	{
		chunk_t expanded, encrypted;
		expanded = ExpandDESKey(keys[i]);
		crypter->set_key(crypter, expanded);
		crypter->encrypt(crypter, challenge_hash, chunk_empty, &encrypted);
		memcpy(&response->ptr[i * 8], encrypted.ptr, encrypted.len);
		chunk_clear(&encrypted);
		chunk_clear(&expanded);
	}
	crypter->destroy(crypter);
	return SUCCESS;
}

/**
 * Computes the authenticator response
 */
static status_t AuthenticatorResponse(chunk_t password_hash_hash,
		chunk_t challenge_hash, chunk_t nt_response, chunk_t *response)
{
	static u_int8_t magic1_data[] =
				{ 0x4D, 0x61, 0x67, 0x69, 0x63, 0x20, 0x73, 0x65, 0x72, 0x76,
				  0x65, 0x72, 0x20, 0x74, 0x6F, 0x20, 0x63, 0x6C, 0x69, 0x65,
				  0x6E, 0x74, 0x20, 0x73, 0x69, 0x67, 0x6E, 0x69, 0x6E, 0x67,
				  0x20, 0x63, 0x6F, 0x6E, 0x73, 0x74, 0x61, 0x6E, 0x74 };
	static u_int8_t magic2_data[] =
				{ 0x50, 0x61, 0x64, 0x20, 0x74, 0x6F, 0x20, 0x6D, 0x61, 0x6B,
				  0x65, 0x20, 0x69, 0x74, 0x20, 0x64, 0x6F, 0x20, 0x6D, 0x6F,
				  0x72, 0x65, 0x20, 0x74, 0x68, 0x61, 0x6E, 0x20, 0x6F, 0x6E,
				  0x65, 0x20, 0x69, 0x74, 0x65, 0x72, 0x61, 0x74, 0x69, 0x6F,
				  0x6E };
	static const chunk_t magic1 = chunk_from_buf(magic1_data);
	static const chunk_t magic2 = chunk_from_buf(magic2_data);
	
	status_t status = FAILED;
	chunk_t digest = chunk_empty, concat;
	hasher_t *hasher;
	
	hasher = lib->crypto->create_hasher(lib->crypto, HASH_SHA1);
	if (hasher == NULL)
	{
		DBG1(DBG_IKE, "EAP-MS-CHAPv2 failed, SHA1 not supported");
		return FAILED;
	}
	
	concat = chunk_cata("ccc", password_hash_hash, nt_response, magic1);
	hasher->allocate_hash(hasher, concat, &digest);
	concat = chunk_cata("ccc", digest, challenge_hash, magic2);
	hasher->allocate_hash(hasher, concat, response);
	
	hasher->destroy(hasher);
	chunk_free(&digest);
	return SUCCESS;
}

/**
 * Generate the master session key according to RFC3079
 */
static status_t GenerateMSK(chunk_t password_hash_hash,
							chunk_t nt_response, chunk_t *msk)
{
	static u_int8_t magic1_data[] =
		{ 0x54, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x74,
		  0x68, 0x65, 0x20, 0x4d, 0x50, 0x50, 0x45, 0x20, 0x4d,
		  0x61, 0x73, 0x74, 0x65, 0x72, 0x20, 0x4b, 0x65, 0x79 };
	static u_int8_t magic2_data[] =
		{ 0x4f, 0x6e, 0x20, 0x74, 0x68, 0x65, 0x20, 0x63, 0x6c, 0x69,
		  0x65, 0x6e, 0x74, 0x20, 0x73, 0x69, 0x64, 0x65, 0x2c, 0x20,
		  0x74, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x74, 0x68,
		  0x65, 0x20, 0x73, 0x65, 0x6e, 0x64, 0x20, 0x6b, 0x65, 0x79,
		  0x3b, 0x20, 0x6f, 0x6e, 0x20, 0x74, 0x68, 0x65, 0x20, 0x73,
		  0x65, 0x72, 0x76, 0x65, 0x72, 0x20, 0x73, 0x69, 0x64, 0x65,
		  0x2c, 0x20, 0x69, 0x74, 0x20, 0x69, 0x73, 0x20, 0x74, 0x68,
		  0x65, 0x20, 0x72, 0x65, 0x63, 0x65, 0x69, 0x76, 0x65, 0x20,
		  0x6b, 0x65, 0x79, 0x2e };
	static u_int8_t magic3_data[] =
		{ 0x4f, 0x6e, 0x20, 0x74, 0x68, 0x65, 0x20, 0x63, 0x6c, 0x69,
		  0x65, 0x6e, 0x74, 0x20, 0x73, 0x69, 0x64, 0x65, 0x2c, 0x20,
		  0x74, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x74, 0x68,
		  0x65, 0x20, 0x72, 0x65, 0x63, 0x65, 0x69, 0x76, 0x65, 0x20,
		  0x6b, 0x65, 0x79, 0x3b, 0x20, 0x6f, 0x6e, 0x20, 0x74, 0x68,
		  0x65, 0x20, 0x73, 0x65, 0x72, 0x76, 0x65, 0x72, 0x20, 0x73,
		  0x69, 0x64, 0x65, 0x2c, 0x20, 0x69, 0x74, 0x20, 0x69, 0x73,
		  0x20, 0x74, 0x68, 0x65, 0x20, 0x73, 0x65, 0x6e, 0x64, 0x20,
		  0x6b, 0x65, 0x79, 0x2e };
	static u_int8_t shapad1_data[] =
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	static u_int8_t shapad2_data[] =
		{ 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2,
		  0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2,
		  0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2,
		  0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2 };
	static const chunk_t magic1 = chunk_from_buf(magic1_data);
	static const chunk_t magic2 = chunk_from_buf(magic2_data);
	static const chunk_t magic3 = chunk_from_buf(magic3_data);
	static const chunk_t shapad1 = chunk_from_buf(shapad1_data);
	static const chunk_t shapad2 = chunk_from_buf(shapad2_data);
	static const chunk_t keypad = { shapad1_data, 16 };
	
	chunk_t concat, master_key, master_receive_key, master_send_key;
	hasher_t *hasher;
	
	hasher = lib->crypto->create_hasher(lib->crypto, HASH_SHA1);
	if (hasher == NULL)
	{
		DBG1(DBG_IKE, "EAP-MS-CHAPv2 failed, SHA1 not supported");
		return FAILED;
	}
	
	concat = chunk_cata("ccc", password_hash_hash, nt_response, magic1);
	hasher->allocate_hash(hasher, concat, &master_key);
	master_key.len = 16;
	
	concat = chunk_cata("cccc", master_key, shapad1, magic2, shapad2);
	hasher->allocate_hash(hasher, concat, &master_receive_key);
	master_receive_key.len = 16;
	
	concat = chunk_cata("cccc", master_key, shapad1, magic3, shapad2);
	hasher->allocate_hash(hasher, concat, &master_send_key);
	master_send_key.len = 16;
	
	*msk = chunk_cat("cccc", master_receive_key, keypad, master_send_key, keypad);
	
	hasher->destroy(hasher);
	chunk_free(&master_key);
	chunk_free(&master_receive_key);
	chunk_free(&master_send_key);
	return SUCCESS;
}

static status_t GenerateStuff(private_eap_mschapv2_t *this,
							  chunk_t server_challenge, chunk_t peer_challenge,
							  chunk_t username, chunk_t password)
{
	status_t status = FAILED;
	chunk_t password_hash = chunk_empty, password_hash_hash = chunk_empty,
			challenge_hash = chunk_empty;
	
	if (NtPasswordHash(password, &password_hash) != SUCCESS)
	{
		goto error;
	}
	if (NtPasswordHash(password_hash, &password_hash_hash) != SUCCESS)
	{
		goto error;
	}
	if (ChallengeHash(peer_challenge, server_challenge, username,
					  &challenge_hash) != SUCCESS)
	{
		goto error;
	}
	
	if (ChallengeResponse(challenge_hash, password_hash,
						  &this->nt_response) != SUCCESS)
	{
		goto error;
	}
	if (AuthenticatorResponse(password_hash_hash, challenge_hash,
							  this->nt_response, &this->auth_response) != SUCCESS)
	{
		goto error;
	}
	if (GenerateMSK(password_hash_hash, this->nt_response, &this->msk) != SUCCESS)
	{
		goto error;
	}
	
	status = SUCCESS;
	
error:
	chunk_free(&password_hash);
	chunk_free(&password_hash_hash);
	chunk_free(&challenge_hash);
	return status;
}

/**
 * Converts an ASCII string into a UTF-16 (little-endian) string
 */
static chunk_t ascii_to_unicode(chunk_t ascii)
{
	int i;
	chunk_t unicode = chunk_alloc(ascii.len * 2);
	for (i = 0; i < ascii.len; i++)
	{
		unicode.ptr[i * 2] = ascii.ptr[i];
		unicode.ptr[i * 2 + 1] = 0;
	}
	return unicode;
}

/**
 * Returns a chunk of just the username part of the given user identity.
 * Note: the chunk points to internal data of the identification.
 */
static chunk_t extract_username(identification_t* identification)
{
	char *has_domain;
	chunk_t id;
	id = identification->get_encoding(identification);
	has_domain = (char*)memrchr(id.ptr, '\\', id.len);
	if (has_domain)
	{
		int len;
		has_domain++; /* skip the backslash */
		len = id.len - ((u_char*)has_domain - id.ptr);
		return len > 0 ? chunk_create(has_domain, len) : chunk_empty;
	}
	return id;
}


/**
 * Implementation of eap_method_t.initiate for the peer
 */
static status_t initiate_peer(private_eap_mschapv2_t *this, eap_payload_t **out)
{
	/* peer never initiates */
	return FAILED;
}

/**
 * Implementation of eap_method_t.initiate for the server
 */
static status_t initiate_server(private_eap_mschapv2_t *this, eap_payload_t **out)
{
	rng_t *rng;
	eap_mschapv2_header_t *eap;
	eap_mschapv2_challenge_t *cha;
	const char *name = MSCHAPV2_HOST_NAME;
	u_int16_t len = CHALLENGE_PAYLOAD_LEN + sizeof(MSCHAPV2_HOST_NAME) - 1;
	
	DBG1(DBG_IKE, "initiating EAP-MS-CHAPv2");
	
	rng = lib->crypto->create_rng(lib->crypto, RNG_WEAK);
	if (!rng)
	{
		DBG1(DBG_IKE, "EAP-MS-CHAPv2 failed, no RNG");
		return FAILED;
	}
	rng->allocate_bytes(rng, CHALLENGE_LEN, &this->challenge);
	rng->destroy(rng);
	
	eap = alloca(len);
	eap->code = EAP_REQUEST;
	eap->identifier = this->identifier;
	eap->length = htons(len);
	eap->type = EAP_MSCHAPV2;
	eap->opcode = MSCHAPV2_CHALLENGE;
	eap->ms_chapv2_id = this->mschapv2id;
	eap->ms_length = htons(len - 5);
	
	cha = (eap_mschapv2_challenge_t*)eap->data;
	cha->value_size = CHALLENGE_LEN;
	memcpy(cha->challenge, this->challenge.ptr, this->challenge.len);
	memcpy(cha->name, name, sizeof(MSCHAPV2_HOST_NAME) - 1);
	
	*out = eap_payload_create_data(chunk_create((void*) eap, len));
	return NEED_MORE;
}


/**
 * Process MS-CHAPv2 Challenge Requests
 */
static status_t process_peer_challenge(private_eap_mschapv2_t *this,
									   eap_payload_t *in, eap_payload_t **out)
{
	rng_t *rng;
	eap_mschapv2_header_t *eap;
	eap_mschapv2_challenge_t *cha;
	eap_mschapv2_response_t *res;
	shared_key_t *shared;
	chunk_t data, peer_challenge, username, password;
	u_int16_t len = RESPONSE_PAYLOAD_LEN;
	
	data = in->get_data(in);
	eap = (eap_mschapv2_header_t*)data.ptr;
	
	/* the name MUST be at least one octet long */
	if (data.len < CHALLENGE_PAYLOAD_LEN + 1)
	{
		DBG1(DBG_IKE, "received invalid EAP-MS-CHAPv2 message: too short");
		return FAILED;
	}
	
	cha = (eap_mschapv2_challenge_t*)eap->data;
			
	if (cha->value_size != CHALLENGE_LEN)
	{
		DBG1(DBG_IKE, "received invalid EAP-MS-CHAPv2 message: invalid challenge size");
		return FAILED;
	}
			
	this->mschapv2id = eap->ms_chapv2_id;
	this->challenge = chunk_clone(chunk_create(cha->challenge, CHALLENGE_LEN));
			
	rng = lib->crypto->create_rng(lib->crypto, RNG_WEAK);
	if (!rng)
	{
		DBG1(DBG_IKE, "EAP-MS-CHAPv2 failed, no RNG");
		return FAILED;
	}
	peer_challenge = chunk_alloca(CHALLENGE_LEN);
	rng->get_bytes(rng, CHALLENGE_LEN, peer_challenge.ptr);
	rng->destroy(rng);
			
	shared = charon->credentials->get_shared(charon->credentials,
											 SHARED_EAP, this->server, this->peer);
	if (shared == NULL)
	{
		DBG1(DBG_IKE, "no EAP key found for hosts '%D' - '%D'",
				this->server, this->peer);
		return NOT_FOUND;
	}
	
	password = ascii_to_unicode(shared->get_key(shared));
	shared->destroy(shared);
	
	username = extract_username(this->peer);
	len += username.len;
			
	if (GenerateStuff(this, this->challenge, peer_challenge, username, password) != SUCCESS)
	{
		DBG1(DBG_IKE, "EAP-MS-CHAPv2 generating NT-Response failed");
		chunk_clear(&password);
		return FAILED;
	}
	chunk_clear(&password);
			
	eap = alloca(len);
	eap->code = EAP_RESPONSE;
	eap->identifier = this->identifier;
	eap->length = htons(len);
	eap->type = EAP_MSCHAPV2;
	eap->opcode = MSCHAPV2_RESPONSE;
	eap->ms_chapv2_id = this->mschapv2id;
	eap->ms_length = htons(len - 5);
	
	res = (eap_mschapv2_response_t*)eap->data;
	res->value_size = RESPONSE_LEN;
	memset(&res->response, 0, RESPONSE_LEN);
	memcpy(res->response.peer_challenge, peer_challenge.ptr, peer_challenge.len);
	memcpy(res->response.nt_response, this->nt_response.ptr, this->nt_response.len);
	
	username = this->peer->get_encoding(this->peer);
	memcpy(res->name, username.ptr, username.len);
	
	*out = eap_payload_create_data(chunk_create((void*) eap, len));
	return NEED_MORE;
}

/**
 * Process MS-CHAPv2 Success Requests
 */
static status_t process_peer_success(private_eap_mschapv2_t *this,
									  eap_payload_t *in, eap_payload_t **out)
{
	status_t status = FAILED;
	enumerator_t *enumerator;
	eap_mschapv2_header_t *eap;
	chunk_t data, auth_string = chunk_empty;
	char *message, *token, *msg = NULL;
	int message_len;
	u_int16_t len = SHORT_HEADER_LEN;
	
	data = in->get_data(in);
	eap = (eap_mschapv2_header_t*)data.ptr;
	
	if (data.len < AUTH_RESPONSE_LEN)
	{
		DBG1(DBG_IKE, "received invalid EAP-MS-CHAPv2 message: too short");
		return FAILED;
	}
	
	message_len = data.len - HEADER_LEN;
	message = malloc(message_len + 1);
	memcpy(message, eap->data, message_len);
	message[message_len] = '\0';
	
	/* S=<auth_string> M=<msg> */
	enumerator = enumerator_create_token(message, " ", " ");
	while (enumerator->enumerate(enumerator, &token))
	{
		if (strneq(token, "S=", 2))
		{
			chunk_t hex;
			token += 2;
			if (strlen(token) != AUTH_RESPONSE_LEN - 2)
			{
				DBG1(DBG_IKE, "received invalid EAP-MS-CHAPv2 message: invalid auth string");
				goto error;
			}
			hex = chunk_create(token, AUTH_RESPONSE_LEN - 2);
			auth_string = chunk_from_hex(hex, NULL);
		}
		else if (strneq(token, "M=", 2))
		{
			token += 2;
			msg = strdup(token);
		}
	}
	enumerator->destroy(enumerator);
			
	if (auth_string.ptr == NULL)	
	{
		DBG1(DBG_IKE, "received invalid EAP-MS-CHAPv2 message: auth string missing");
		goto error;
	}
	
	if (!chunk_equals(this->auth_response, auth_string))
	{
		DBG1(DBG_IKE, "EAP-MS-CHAPv2 verification failed");
		goto error;
	}
	
	DBG1(DBG_IKE, "EAP-MS-CHAPv2 succeeded: '%s'", msg);
	
	eap = alloca(len);
	eap->code = EAP_RESPONSE;
	eap->identifier = this->identifier;
	eap->length = htons(len);
	eap->type = EAP_MSCHAPV2;
	eap->opcode = MSCHAPV2_SUCCESS;
	
	*out = eap_payload_create_data(chunk_create((void*) eap, len));
	status = NEED_MORE;
	
error:
	chunk_free(&auth_string);
	free(message);
	free(msg);
	return status;
}

static status_t process_peer_failure(private_eap_mschapv2_t *this,
									 eap_payload_t *in, eap_payload_t **out)
{
	status_t status = FAILED;
	enumerator_t *enumerator;
	eap_mschapv2_header_t *eap;
	chunk_t data;
	char *message, *token, *msg = NULL;
	int message_len, error, retryable;
	chunk_t challenge = chunk_empty;
	u_int16_t len = SHORT_HEADER_LEN;
	
	data = in->get_data(in);
	eap = (eap_mschapv2_header_t*)data.ptr;
	
	if (data.len < 3) /* we want at least an error code: E=e */
	{
		DBG1(DBG_IKE, "received invalid EAP-MS-CHAPv2 message: too short");
		return FAILED;
	}
	
	message_len = data.len - HEADER_LEN;
	message = malloc(message_len + 1);
	memcpy(message, eap->data, message_len);
	message[message_len] = '\0';
			
	/* E=eeeeeeeeee R=r C=cccccccccccccccccccccccccccccccc V=vvvvvvvvvv M=<msg> */
	enumerator = enumerator_create_token(message, " ", " ");
	while (enumerator->enumerate(enumerator, &token))
	{
		if (strneq(token, "E=", 2))
		{
			token += 2;
			error = atoi(token);
		}
		else if (strneq(token, "R=", 2))
		{
			token += 2;
			retryable = atoi(token);
		}
		else if (strneq(token, "C=", 2))
		{
			chunk_t hex;
			token += 2;
			if (strlen(token) != 2 * CHALLENGE_LEN)
			{
				DBG1(DBG_IKE, "received invalid EAP-MS-CHAPv2 message: invalid challenge");
				goto error;
			}
			hex = chunk_create(token, 2 * CHALLENGE_LEN);
			challenge = chunk_from_hex(hex, NULL);
		}
		else if (strneq(token, "V=", 2))
		{
			int version;
			token += 2;
			version = atoi(token);
		}
		else if (strneq(token, "M=", 2))
		{
			token += 2;
			msg = strdup(token);
		}
	}
	enumerator->destroy(enumerator);
			
	DBG1(DBG_IKE, "EAP-MS-CHAPv2 failed with error %N: '%s'", mschapv2_error_names, error, msg);
			
	/**
	 * at this point, if the error is retryable, we MAY retry the authentication
	 * or MAY send a Change Password packet.
	 * 
	 * if the error is not retryable (or if we do neither of the above), we
	 * SHOULD send a Failure Response packet.
	 * windows clients don't do that, and since windows server 2008 r2 behaves
	 * pretty odd if we do send a Failure Response, we just don't send one
	 * either. windows 7 actually sends a delete notify (which, according to the
	 * logs, results in an error on windows server 2008 r2). 
	 * 
	 * btw, windows server 2008 r2 does not send non-retryable errors for e.g.
	 * a disabled account but returns the windows error code in a notify payload
	 * of type 12345.
	 */
	
	status = FAILED;
	
error:
	chunk_free(&challenge);
	free(message);
	free(msg);
	return status;
}

/**
 * Implementation of eap_method_t.process for the peer
 */
static status_t process_peer(private_eap_mschapv2_t *this, eap_payload_t *in,
		eap_payload_t **out)
{
	chunk_t data;
	eap_mschapv2_header_t *eap;
	
	this->identifier = in->get_identifier(in);
	data = in->get_data(in);
	if (data.len < SHORT_HEADER_LEN)
	{
		DBG1(DBG_IKE, "received invalid EAP-MS-CHAPv2 message");
		return FAILED;
	}
	
	eap = (eap_mschapv2_header_t*)data.ptr;
		
	switch (eap->opcode)
	{
		case MSCHAPV2_CHALLENGE:
		{
			return process_peer_challenge(this, in, out);
		}
		case MSCHAPV2_SUCCESS:
		{
			return process_peer_success(this, in, out);
		}
		case MSCHAPV2_FAILURE:
		{
			return process_peer_failure(this, in, out);
		}
		default:
		{
			DBG1(DBG_IKE, "EAP-MS-CHAPv2 received packet with unsupported OpCode (%N)!",
					mschapv2_opcode_names, eap->opcode);
			break;
		}
	}
	return FAILED;
}

/**
 * Handles retries on the server
 */
static status_t process_server_retry(private_eap_mschapv2_t *this,
									 eap_payload_t **out)
{
	eap_mschapv2_header_t *eap;
	rng_t *rng;
	chunk_t hex;
	char msg[FAILURE_MESSAGE_LEN];
	u_int16_t len = HEADER_LEN + FAILURE_MESSAGE_LEN - 1; /* no null byte */
	
	if (++this->retries > MAX_RETRIES)
	{
		/* we MAY send a Failure Request with R=0, but windows 7 does not
		 * really like that and does not respond with a Failure Response.
		 * so, to clean up our state we just fail with an EAP-Failure.
		 * this gives an unknown error on the windows side, but is also fine
		 * with the standard. */
		DBG1(DBG_IKE, "EAP-MS-CHAPv2 verification failed: maximum number of retries reached");
		return FAILED;
	}
	
	DBG1(DBG_IKE, "EAP-MS-CHAPv2 verification failed, retry (%d)", this->retries);
	
	rng = lib->crypto->create_rng(lib->crypto, RNG_WEAK);
	if (!rng)
	{
		DBG1(DBG_IKE, "EAP-MS-CHAPv2 failed, no RNG");
		return FAILED;
	}
	rng->get_bytes(rng, CHALLENGE_LEN, this->challenge.ptr);
	rng->destroy(rng);
	
	chunk_free(&this->nt_response);
	chunk_free(&this->auth_response);
	chunk_free(&this->msk);
	
	eap = alloca(len);
	eap->code = EAP_REQUEST;
	eap->identifier = ++this->identifier;
	eap->length = htons(len);
	eap->type = EAP_MSCHAPV2;
	eap->opcode = MSCHAPV2_FAILURE;
	eap->ms_chapv2_id = this->mschapv2id++; /* increase for each retry */
	eap->ms_length = htons(len - 5);
	
	hex = chunk_to_hex(this->challenge, NULL, TRUE);
	snprintf(msg, FAILURE_MESSAGE_LEN, "%s%s", FAILURE_MESSAGE, hex.ptr);
	chunk_free(&hex);
	memcpy(eap->data, msg, FAILURE_MESSAGE_LEN - 1); /* no null byte */
	*out = eap_payload_create_data(chunk_create((void*) eap, len));
	
	/* delay the response for some time to make brute-force attacks harder */
	sleep(RETRY_DELAY);
	
	return NEED_MORE;
}

/**
 * Process MS-CHAPv2 Response response packets
 */
static status_t process_server_response(private_eap_mschapv2_t *this,
										eap_payload_t *in, eap_payload_t **out)
{
	eap_mschapv2_header_t *eap;
	eap_mschapv2_response_t *res;
	chunk_t data, peer_challenge, username, password;
	identification_t *userid;
	shared_key_t *shared;
	int name_len;
	
	data = in->get_data(in);
	eap = (eap_mschapv2_header_t*)data.ptr;
	
	if (data.len < RESPONSE_PAYLOAD_LEN)
	{
		DBG1(DBG_IKE, "received invalid EAP-MS-CHAPv2 message: too short");
		return FAILED;
	}
	
	res = (eap_mschapv2_response_t*)eap->data;
	peer_challenge = chunk_create(res->response.peer_challenge, CHALLENGE_LEN);
	
	name_len = min(data.len - RESPONSE_PAYLOAD_LEN, 255);
	userid = identification_create_from_encoding(ID_EAP,
												 chunk_create(res->name, name_len));
	username = extract_username(userid);
	DBG2(DBG_IKE, "EAP-MS-CHAPv2 username: '%.*s'", name_len, res->name);
	
	shared = charon->credentials->get_shared(charon->credentials,
											 SHARED_EAP, this->server, userid);
	if (shared == NULL)
	{
		DBG1(DBG_IKE, "no EAP key found for hosts '%D' - '%D'",
					  this->server, userid);
		/* FIXME: windows 7 always sends the username that is first entered in
		 * the username box, even, if the user changes it during retries (probably
		 * to keep consistent with the EAP-Identity).
	 	 * thus, we could actually fail here, because retries do not make much
	 	 * sense. on the other hand, an attacker could guess usernames, if the
	 	 * error messages were different. */
		userid->destroy(userid);
		return process_server_retry(this, out);
	}
	
	password = ascii_to_unicode(shared->get_key(shared));
	shared->destroy(shared);
	
	if (GenerateStuff(this, this->challenge, peer_challenge, username, password) != SUCCESS)
	{
		DBG1(DBG_IKE, "EAP-MS-CHAPv2 verification failed");	
		userid->destroy(userid);
		chunk_clear(&password);
		return FAILED;
	}
	userid->destroy(userid);
	chunk_clear(&password);
	
	if (memeq(res->response.nt_response, this->nt_response.ptr, this->nt_response.len))
	{
		chunk_t hex;
		char msg[AUTH_RESPONSE_LEN + sizeof(SUCCESS_MESSAGE)];
		u_int16_t len = HEADER_LEN + AUTH_RESPONSE_LEN + sizeof(SUCCESS_MESSAGE);
		
		eap = alloca(len);
		eap->code = EAP_REQUEST;
		eap->identifier = ++this->identifier;
		eap->length = htons(len);
		eap->type = EAP_MSCHAPV2;
		eap->opcode = MSCHAPV2_SUCCESS;
		eap->ms_chapv2_id = this->mschapv2id;
		eap->ms_length = htons(len - 5);
		
		hex = chunk_to_hex(this->auth_response, NULL, TRUE);
		snprintf(msg, AUTH_RESPONSE_LEN + sizeof(SUCCESS_MESSAGE),
					  "S=%s%s", hex.ptr, SUCCESS_MESSAGE);
		chunk_free(&hex);
		memcpy(eap->data, msg, AUTH_RESPONSE_LEN + sizeof(SUCCESS_MESSAGE));
		*out = eap_payload_create_data(chunk_create((void*) eap, len));
		return NEED_MORE;
	}

	return process_server_retry(this, out);
}

/**
 * Implementation of eap_method_t.process for the server
 */
static status_t process_server(private_eap_mschapv2_t *this, eap_payload_t *in,
		eap_payload_t **out)
{
	eap_mschapv2_header_t *eap;
	chunk_t data;
	
	if (this->identifier != in->get_identifier(in))
	{
		DBG1(DBG_IKE, "received invalid EAP-MS-CHAPv2 message: unexpected identifier");
		return FAILED;
	}
	
	data = in->get_data(in);
	if (data.len < SHORT_HEADER_LEN)
	{
		DBG1(DBG_IKE, "received invalid EAP-MS-CHAPv2 message: too short");
		return FAILED;
	}
	
	eap = (eap_mschapv2_header_t*)data.ptr;
	
	switch (eap->opcode)
	{
		case MSCHAPV2_RESPONSE:
		{
			return process_server_response(this, in, out);
		}
		case MSCHAPV2_SUCCESS:
		{
			return SUCCESS;
		}
		case MSCHAPV2_FAILURE:
		{
			return FAILED;
		}
		default:
		{
			DBG1(DBG_IKE, "EAP-MS-CHAPv2 received packet with unsupported OpCode (%N)!",
					mschapv2_opcode_names, eap->opcode);
			break;
		}
	}
	return FAILED;
}

/**
 * Implementation of eap_method_t.get_type.
 */
static eap_type_t get_type(private_eap_mschapv2_t *this, u_int32_t *vendor)
{
	*vendor = 0;
	return EAP_MSCHAPV2;
}

/**
 * Implementation of eap_method_t.get_msk.
 */
static status_t get_msk(private_eap_mschapv2_t *this, chunk_t *msk)
{
	if (this->msk.ptr)
	{
		*msk = this->msk;
		return SUCCESS;
	}
	return FAILED;
}

/**
 * Implementation of eap_method_t.is_mutual.
 */
static bool is_mutual(private_eap_mschapv2_t *this)
{
	return TRUE;
}

/**
 * Implementation of eap_method_t.destroy.
 */
static void destroy(private_eap_mschapv2_t *this)
{
	this->peer->destroy(this->peer);
	this->server->destroy(this->server);
	chunk_free(&this->challenge);
	chunk_free(&this->nt_response);
	chunk_free(&this->auth_response);
	chunk_free(&this->msk);
	free(this);
}

/**
 * Generic constructor
 */
static private_eap_mschapv2_t *eap_mschapv2_create_generic(identification_t *server, identification_t *peer)
{
	private_eap_mschapv2_t *this = malloc_thing(private_eap_mschapv2_t);
	
	this->public.eap_method_interface.initiate = NULL;
	this->public.eap_method_interface.process = NULL;
	this->public.eap_method_interface.get_type = (eap_type_t(*)(eap_method_t*,u_int32_t*))get_type;
	this->public.eap_method_interface.is_mutual = (bool(*)(eap_method_t*))is_mutual;
	this->public.eap_method_interface.get_msk = (status_t(*)(eap_method_t*,chunk_t*))get_msk;
	this->public.eap_method_interface.destroy = (void(*)(eap_method_t*))destroy;
	
	/* private data */
	this->peer = peer->clone(peer);
	this->server = server->clone(server);
	this->challenge = chunk_empty;
	this->nt_response = chunk_empty;
	this->auth_response = chunk_empty;
	this->msk = chunk_empty;
	this->identifier = 0;
	this->mschapv2id = 0;
	this->retries = 0;
	
	return this;
}

/*
 * see header
 */
eap_mschapv2_t *eap_mschapv2_create_server(identification_t *server, identification_t *peer)
{
	private_eap_mschapv2_t *this = eap_mschapv2_create_generic(server, peer);
	
	this->public.eap_method_interface.initiate = (status_t(*)(eap_method_t*,eap_payload_t**))initiate_server;
	this->public.eap_method_interface.process = (status_t(*)(eap_method_t*,eap_payload_t*, eap_payload_t**))process_server;

	/* generate a non-zero identifier */
	do
	{
		this->identifier = random();
	} while (!this->identifier);
	
	this->mschapv2id = this->identifier;
	
	return &this->public;
}

/*
 * see header
 */
eap_mschapv2_t *eap_mschapv2_create_peer(identification_t *server, identification_t *peer)
{
	private_eap_mschapv2_t *this = eap_mschapv2_create_generic(server, peer);
	
	this->public.eap_method_interface.initiate = (status_t(*)(eap_method_t*,eap_payload_t**))initiate_peer;
	this->public.eap_method_interface.process = (status_t(*)(eap_method_t*,eap_payload_t*, eap_payload_t**))process_peer;
	
	return &this->public;
}

