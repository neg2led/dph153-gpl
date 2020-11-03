/*
 * Copyright (C) 2001-2008 Andreas Steffen
 *
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
 * $Id: pem.c 4029 2008-06-03 12:14:02Z martin $
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stddef.h>
#include <sys/types.h>

#include "pem.h"

#include <library.h>
#include <debug.h>
#include <asn1/asn1.h>

#include <utils/lexparser.h>
#include <crypto/hashers/hasher.h>
#include <crypto/crypters/crypter.h>

#define PKCS5_SALT_LEN	8	/* bytes */

/**
 * check the presence of a pattern in a character string
 */
static bool present(const char* pattern, chunk_t* ch)
{
	u_int pattern_len = strlen(pattern);

	if (ch->len >= pattern_len && strneq(ch->ptr, pattern, pattern_len))
	{
		ch->ptr += pattern_len;
		ch->len -= pattern_len;
		return TRUE;
	}
	return FALSE;
}

/**
 * find a boundary of the form -----tag name-----
 */
static bool find_boundary(const char* tag, chunk_t *line)
{
	chunk_t name = chunk_empty;

	if (!present("-----", line))
		return FALSE;
	if (!present(tag, line))
		return FALSE;
	if (*line->ptr != ' ')
		return FALSE;
	line->ptr++;  line->len--;
	
	/* extract name */
	name.ptr = line->ptr;
	while (line->len > 0)
	{
		if (present("-----", line))
		{
			DBG2("  -----%s %.*s-----", tag, (int)name.len, name.ptr);
			return TRUE;
		}
		line->ptr++;  line->len--;  name.len++;
	}
	return FALSE;
}

/*
 * decrypts a passphrase protected encrypted data block
 */
static bool pem_decrypt(chunk_t *blob, encryption_algorithm_t alg, size_t key_size,
						 chunk_t *iv, chunk_t *passphrase)
{
	hasher_t *hasher;
	crypter_t *crypter;
	chunk_t salt = { iv->ptr, PKCS5_SALT_LEN };
	chunk_t hash;
	chunk_t decrypted;
	chunk_t key = {alloca(key_size), key_size};
	u_int8_t padding, *last_padding_pos, *first_padding_pos;
	
	if (passphrase == NULL || passphrase->len == 0)
	{
		DBG1("  missing passphrase");
		return FALSE;
	}

	/* build key from passphrase and IV */
	hasher = lib->crypto->create_hasher(lib->crypto, HASH_MD5);
	if (hasher == NULL)
	{
		DBG1("  MD5 hash algorithm not available");
		return FALSE;
	}
	hash.len = hasher->get_hash_size(hasher);
	hash.ptr = alloca(hash.len);
	hasher->get_hash(hasher, *passphrase, NULL);
	hasher->get_hash(hasher, salt, hash.ptr);
	memcpy(key.ptr, hash.ptr, hash.len);

	if (key.len > hash.len)
	{
		hasher->get_hash(hasher, hash, NULL);
		hasher->get_hash(hasher, *passphrase, NULL);
		hasher->get_hash(hasher, salt, hash.ptr);	
		memcpy(key.ptr + hash.len, hash.ptr, key.len - hash.len);
	}	
	hasher->destroy(hasher);
	
	/* decrypt blob */
	crypter = lib->crypto->create_crypter(lib->crypto, alg, key_size);
	if (crypter == NULL)
	{
		DBG1("  %N encryption algorithm not available",
			 encryption_algorithm_names, alg);
		return FALSE;
	}
	crypter->set_key(crypter, key);
	
	if (iv->len != crypter->get_block_size(crypter) ||
		blob->len % iv->len)
	{
		crypter->destroy(crypter);
		DBG1("  data size is not multiple of block size");
		return FALSE;
	}
	crypter->decrypt(crypter, *blob, *iv, &decrypted);
	crypter->destroy(crypter);
	memcpy(blob->ptr, decrypted.ptr, blob->len);
	chunk_free(&decrypted);
	
	/* determine amount of padding */
	last_padding_pos = blob->ptr + blob->len - 1;
	padding = *last_padding_pos;
	first_padding_pos = (padding > blob->len) ? blob->ptr : last_padding_pos - padding;

	/* check the padding pattern */
	while (--last_padding_pos > first_padding_pos)
	{
		if (*last_padding_pos != padding)
		{
			DBG1("  invalid passphrase");
			return FALSE;
		}
	}
	/* remove padding */
	blob->len -= padding;
	return TRUE;
}

/*  Converts a PEM encoded file into its binary form
 *
 *  RFC 1421 Privacy Enhancement for Electronic Mail, February 1993
 *  RFC 934 Message Encapsulation, January 1985
 */
bool pem_to_bin(chunk_t *blob, chunk_t *passphrase, bool *pgp)
{
	typedef enum {
		PEM_PRE    = 0,
		PEM_MSG    = 1,
		PEM_HEADER = 2,
		PEM_BODY   = 3,
		PEM_POST   = 4,
		PEM_ABORT  = 5
	} state_t;

	encryption_algorithm_t alg = ENCR_UNDEFINED;
	size_t key_size = 0;

	bool encrypted = FALSE;

	state_t state  = PEM_PRE;

	chunk_t src    = *blob;
	chunk_t dst    = *blob;
	chunk_t line   = chunk_empty;
	chunk_t iv     = chunk_empty;

	u_char iv_buf[16]; /* MD5 digest size */

	/* zero size of converted blob */
	dst.len = 0;

	/* zero size of IV */
	iv.ptr = iv_buf;
	iv.len = 0;

	while (fetchline(&src, &line))
	{
		if (state == PEM_PRE)
		{
			if (find_boundary("BEGIN", &line))
			{
				state = PEM_MSG;
			}
			continue;
		}
		else
		{
			if (find_boundary("END", &line))
			{
				state = PEM_POST;
				break;
			}
			if (state == PEM_MSG)
			{
				state = (memchr(line.ptr, ':', line.len) == NULL) ? PEM_BODY : PEM_HEADER;
			}
			if (state == PEM_HEADER)
			{
				err_t ugh = NULL;
				chunk_t name  = chunk_empty;
				chunk_t value = chunk_empty;

				/* an empty line separates HEADER and BODY */
				if (line.len == 0)
				{
					state = PEM_BODY;
					continue;
				}

				/* we are looking for a parameter: value pair */
				DBG2("  %.*s", (int)line.len, line.ptr);
				ugh = extract_parameter_value(&name, &value, &line);
				if (ugh != NULL)
					continue;

				if (match("Proc-Type", &name) && *value.ptr == '4')
					encrypted = TRUE;
				else if (match("DEK-Info", &name))
				{
					chunk_t dek;

					if (!extract_token(&dek, ',', &value))
						dek = value;

					if (match("DES-EDE3-CBC", &dek))
					{
						alg = ENCR_3DES;
						key_size = 24;
					}
					else if (match("AES-128-CBC", &dek))
					{
						alg = ENCR_AES_CBC;
						key_size = 16;
					}
					else if (match("AES-192-CBC", &dek))
					{
						alg = ENCR_AES_CBC;
						key_size = 24;
					}
					else if (match("AES-256-CBC", &dek))
					{
						alg = ENCR_AES_CBC;
						key_size = 32;
					}
					else
					{
						DBG1("  encryption algorithm '%.s' not supported",
							 dek.len, dek.ptr);
						return FALSE;
					}
					eat_whitespace(&value);
					iv = chunk_from_hex(value, iv.ptr);
				}
			}
			else /* state is PEM_BODY */
			{
				chunk_t data;
				
				/* remove any trailing whitespace */
				if (!extract_token(&data ,' ', &line))
				{
					data = line;
				}
				
				/* check for PGP armor checksum */
				if (*data.ptr == '=')
				{
					*pgp = TRUE;
					data.ptr++;
					data.len--;
					DBG2("  armor checksum: %.*s", (int)data.len, data.ptr);
		    		continue;
				}
				
				if (blob->len - dst.len < data.len / 4 * 3)
				{
					state = PEM_ABORT;
				}
				data = chunk_from_base64(data, dst.ptr);

				dst.ptr += data.len;
				dst.len += data.len;
			}
		}
	}
	/* set length to size of binary blob */
	blob->len = dst.len;

	if (state != PEM_POST)
	{
		DBG1("  file coded in unknown format, discarded");
		return FALSE;
	}
	if (!encrypted)
	{
		return TRUE;
	}
	return pem_decrypt(blob, alg, key_size, &iv, passphrase);
	
}

/* load a coded key or certificate file with autodetection
 * of binary DER or base64 PEM ASN.1 formats and armored PGP format
 */
bool pem_asn1_load_file(char *filename, chunk_t *passphrase,
						chunk_t *blob, bool *pgp)
{
	FILE *fd = fopen(filename, "r");

	if (fd)
	{
		int bytes;
		fseek(fd, 0, SEEK_END );
		blob->len = ftell(fd);
		rewind(fd);
		blob->ptr = malloc(blob->len);
		bytes = fread(blob->ptr, 1, blob->len, fd);
		fclose(fd);
		DBG2("  loading '%s' (%d bytes)", filename, bytes);

		*pgp = FALSE;

		/* try DER format */
		if (is_asn1(*blob))
		{
			DBG2("  file coded in DER format");
			return TRUE;
		}

		if (passphrase != NULL)
			DBG4("  passphrase:", passphrase->ptr, passphrase->len);

		/* try PEM format */
		if (pem_to_bin(blob, passphrase, pgp))
		{
			if (*pgp)
			{
				DBG2("  file coded in armored PGP format");
				return TRUE;
			}
			if (is_asn1(*blob))
			{
				DBG2("  file coded in PEM format");
				return TRUE;
			}
			DBG1("  file coded in unknown format, discarded");
		}

		/* a conversion error has occured */
		chunk_free(blob);
	}
	else
	{
		DBG1("  reading file '%s' failed", filename);
	}
	return FALSE;
}

