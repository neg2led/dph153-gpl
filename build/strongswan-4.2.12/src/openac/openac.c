/**
 * @file openac.c
 * 
 * @brief Generation of X.509 attribute certificates.
 * 
 */

/*
 * Copyright (C) 2002  Ueli Galizzi, Ariane Seiler
 * Copyright (C) 2004,2007  Andreas Steffen
 * Hochschule fuer Technik Rapperswil, Switzerland
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
 * RCSID $Id: openac.c 4749 2008-12-04 04:34:49Z andreas $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <getopt.h>
#include <ctype.h>
#include <time.h>
#include <gmp.h>

#include <library.h>
#include <debug.h>
#include <asn1/asn1.h>
#include <asn1/pem.h>
#include <credentials/certificates/x509.h>
#include <credentials/certificates/ac.h>
#include <utils/optionsfrom.h>

#ifdef INTEGRITY_TEST
#include <fips/fips.h>
#include <fips_signature.h>
#endif /* INTEGRITY_TEST */

#define OPENAC_PATH   		IPSEC_CONFDIR "/openac"
#define OPENAC_SERIAL 		IPSEC_CONFDIR "/openac/serial"

#define DEFAULT_VALIDITY	24*3600		/* seconds */

/**
 * @brief prints the usage of the program to the stderr
 */
static void usage(const char *message)
{
	if (message != NULL && *message != '\0')
	{
		fprintf(stderr, "%s\n", message);
	}
	fprintf(stderr, "Usage: openac"
		" [--help]"
		" [--version]"
		" [--optionsfrom <filename>]"
		" [--quiet]"
		" \\\n\t"
		"      [--debug <level 0..4>]"
		" \\\n\t"
		"      [--days <days>]"
		" [--hours <hours>]"
		" \\\n\t"
		"      [--startdate <YYYYMMDDHHMMSSZ>]"
		" [--enddate <YYYYMMDDHHMMSSZ>]"
		" \\\n\t"
		"      --cert <certfile>"
		" --key <keyfile>"
		" [--password <password>]"
		" \\\n\t"
		"      --usercert <certfile>"
		" --groups <attr1,attr2,..>"
		" --out <filename>"
		"\n"
	);
}


/**
 * convert a chunk into a multi-precision integer
 */
static void chunk_to_mpz(chunk_t chunk, mpz_t number)
{
	mpz_import(number, chunk.len, 1, 1, 1, 0, chunk.ptr);
}

/**
 * convert a multi-precision integer into a chunk
 */
static chunk_t mpz_to_chunk(mpz_t number)
{
	chunk_t chunk;

	chunk.len = 1 + mpz_sizeinbase(number, 2)/BITS_PER_BYTE;
	chunk.ptr = mpz_export(NULL, NULL, 1, chunk.len, 1, 0, number);
	if (chunk.ptr == NULL)
	{
		chunk.len = 0;
	}
	return chunk;
}

/**
 * read the last serial number from file
 */
static chunk_t read_serial(void)
{
	mpz_t number;

	char buf[BUF_LEN], buf1[BUF_LEN];
	chunk_t hex_serial  = { buf, BUF_LEN };
	chunk_t last_serial = { buf1, BUF_LEN };
	chunk_t serial;

	FILE *fd = fopen(OPENAC_SERIAL, "r");

	/* last serial number defaults to 0 */
	*last_serial.ptr = 0x00;
	last_serial.len = 1;

	if (fd)
	{
		if (fscanf(fd, "%s", hex_serial.ptr))
		{
			hex_serial.len = strlen(hex_serial.ptr);
			last_serial = chunk_from_hex(hex_serial, last_serial.ptr);
		}
		fclose(fd);
	}
	else
	{
		DBG1("  file '%s' does not exist yet - serial number set to 01", OPENAC_SERIAL);
	}

	/**
	 * conversion of read serial number to a multiprecision integer
	 * and incrementing it by one
	 * and representing it as a two's complement octet string
	 */
	mpz_init(number);
	chunk_to_mpz(last_serial, number);
	mpz_add_ui(number, number, 0x01);
	serial = mpz_to_chunk(number);
	mpz_clear(number);

	return serial;
}

/**
 * write back the last serial number to file
 */
static void write_serial(chunk_t serial)
{
	FILE *fd = fopen(OPENAC_SERIAL, "w");

	if (fd)
	{
		chunk_t hex_serial;

		DBG1("  serial number is %#B", &serial);
		hex_serial = chunk_to_hex(serial, NULL, FALSE);
		fprintf(fd, "%.*s\n", hex_serial.len, hex_serial.ptr);
		fclose(fd);
		free(hex_serial.ptr);
	}
	else
	{
		DBG1("  could not open file '%s' for writing", OPENAC_SERIAL);
	}
}

/**
 * Load and parse a private key file
 */
static private_key_t* private_key_create_from_file(char *path, chunk_t *secret)
{
	bool pgp = FALSE;
	chunk_t chunk = chunk_empty;
	private_key_t *key = NULL;

	if (!pem_asn1_load_file(path, secret, &chunk, &pgp))
	{
		DBG1("  could not load private key file '%s'", path);
		return NULL;
	}
	key = lib->creds->create(lib->creds, CRED_PRIVATE_KEY, KEY_RSA,
							 BUILD_BLOB_ASN1_DER, chunk, BUILD_END);
	free(chunk.ptr);
	if (key == NULL)
	{
		DBG1("  could not parse loaded private key file '%s'", path);
		return NULL;
	}
	DBG1("  loaded private key file '%s'", path);
	return key;
}

/**
 * global variables accessible by both main() and build.c
 */

static int debug_level = 1;
static bool stderr_quiet = FALSE;

/**
 * openac dbg function
 */
static void openac_dbg(int level, char *fmt, ...)
{
	int priority = LOG_INFO;
	va_list args;
	
	if (level <= debug_level)
	{
		va_start(args, fmt);
		if (!stderr_quiet)
		{
			vfprintf(stderr, fmt, args);
			fprintf(stderr, "\n");
		}
		vsyslog(priority, fmt, args);
		va_end(args);
	}
}

/**
 * @brief openac main program
 *
 * @param argc number of arguments
 * @param argv pointer to the argument values
 */
int main(int argc, char **argv)
{
	certificate_t *attr_cert   = NULL;
	certificate_t *userCert   = NULL;
	certificate_t *signerCert = NULL;
	private_key_t *signerKey  = NULL;

	time_t notBefore = UNDEFINED_TIME;
	time_t notAfter  = UNDEFINED_TIME;
	time_t validity = 0;

	char *keyfile = NULL;
	char *certfile = NULL;
	char *usercertfile = NULL;
	char *outfile = NULL;
	char *groups = "";
	char buf[BUF_LEN];

	chunk_t passphrase = { buf, 0 };
	chunk_t serial = chunk_empty;
	chunk_t attr_chunk = chunk_empty;

	int status = 1;
	
	/* enable openac debugging hook */
	dbg = openac_dbg;

	passphrase.ptr[0] = '\0';

	openlog("openac", 0, LOG_AUTHPRIV);

	/* initialize library */
	library_init(STRONGSWAN_CONF);
	lib->plugins->load(lib->plugins, IPSEC_PLUGINDIR, 
		lib->settings->get_str(lib->settings, "openac.load", PLUGINS));

	/* initialize optionsfrom */
	options_t *options = options_create();

	/* handle arguments */
	for (;;)
	{
		static const struct option long_opts[] = {
			/* name, has_arg, flag, val */
			{ "help", no_argument, NULL, 'h' },
			{ "version", no_argument, NULL, 'v' },
			{ "optionsfrom", required_argument, NULL, '+' },
			{ "quiet", no_argument, NULL, 'q' },
			{ "cert", required_argument, NULL, 'c' },
			{ "key", required_argument, NULL, 'k' },
			{ "password", required_argument, NULL, 'p' },
			{ "usercert", required_argument, NULL, 'u' },
			{ "groups", required_argument, NULL, 'g' },
			{ "days", required_argument, NULL, 'D' },
			{ "hours", required_argument, NULL, 'H' },
			{ "startdate", required_argument, NULL, 'S' },
			{ "enddate", required_argument, NULL, 'E' },
			{ "out", required_argument, NULL, 'o' },
			{ "debug", required_argument, NULL, 'd' },
			{ 0,0,0,0 }
		};
	
		int c = getopt_long(argc, argv, "hv+:qc:k:p;u:g:D:H:S:E:o:d:", long_opts, NULL);

		/* Note: "breaking" from case terminates loop */
		switch (c)
		{
			case EOF:	/* end of flags */
				break;

			case 0: /* long option already handled */
		 		continue;

			case ':':	/* diagnostic already printed by getopt_long */
			case '?':	/* diagnostic already printed by getopt_long */
			case 'h':	/* --help */
				usage(NULL);
				status = 1;
				goto end;

			case 'v':	/* --version */
				printf("openac (strongSwan %s)\n", VERSION);
				status = 0;
				goto end;

			case '+':	/* --optionsfrom <filename> */
				{
					char path[BUF_LEN];

					if (*optarg == '/')	/* absolute pathname */
					{
		    			strncpy(path, optarg, BUF_LEN);
					}
					else			/* relative pathname */
					{
		    			snprintf(path, BUF_LEN, "%s/%s", OPENAC_PATH, optarg);
					}
					if (!options->from(options, path, &argc, &argv, optind))
					{
						status = 1;
						goto end;
					}
		 		}
				continue;

			case 'q':	/* --quiet */
				stderr_quiet = TRUE;
				continue;

			case 'c':	/* --cert */
				certfile = optarg;
				continue;

			case 'k':	/* --key */
				keyfile = optarg;
				continue;

			case 'p':	/* --key */
				if (strlen(optarg) > BUF_LEN)
				{
					usage("passphrase too long");
					goto end;
				}
				strncpy(passphrase.ptr, optarg, BUF_LEN);
				passphrase.len = min(strlen(optarg), BUF_LEN);
				continue;

			case 'u':	/* --usercert */
				usercertfile = optarg;
				continue;

			case 'g':	/* --groups */
				groups = optarg;
				continue;

			case 'D':	/* --days */
				if (optarg == NULL || !isdigit(optarg[0]))
				{
					usage("missing number of days");
					goto end;
				}
				else
				{
					char *endptr;
					long days = strtol(optarg, &endptr, 0);

					if (*endptr != '\0' || endptr == optarg || days <= 0)
					{
						usage("<days> must be a positive number");
						goto end;
					}
					validity += 24*3600*days;
				}
				continue;

			case 'H':	/* --hours */
				if (optarg == NULL || !isdigit(optarg[0]))
				{
					usage("missing number of hours");
					goto end;
				}
				else
				{
					char *endptr;
					long hours = strtol(optarg, &endptr, 0);

					if (*endptr != '\0' || endptr == optarg || hours <= 0)
					{
						usage("<hours> must be a positive number");
						goto end;
					}
					validity += 3600*hours;
				}
				continue;

			case 'S':	/* --startdate */
				if (optarg == NULL || strlen(optarg) != 15 || optarg[14] != 'Z')
				{
					usage("date format must be YYYYMMDDHHMMSSZ");
					goto end;
				}
				else
				{
					chunk_t date = { optarg, 15 };

					notBefore = asn1_to_time(&date, ASN1_GENERALIZEDTIME);
				}
				continue;

			case 'E':	/* --enddate */
				if (optarg == NULL || strlen(optarg) != 15 || optarg[14] != 'Z')
				{
					usage("date format must be YYYYMMDDHHMMSSZ");
					goto end;
				}
				else
				{
					chunk_t date = { optarg, 15 };
					notAfter = asn1_to_time(&date, ASN1_GENERALIZEDTIME);
				}
				continue;

			case 'o':	/* --out */
				outfile = optarg;
				continue;

			case 'd':	/* --debug */
				debug_level = atoi(optarg);
				continue;

			default:
				usage("");
				status = 0;
				goto end;
		}
		/* break from loop */
		break;
	}

	if (optind != argc)
	{
		usage("unexpected argument");
		goto end;
	}

	DBG1("starting openac (strongSwan Version %s)", VERSION);

#ifdef INTEGRITY_TEST
	DBG1("integrity test of libstrongswan code");
	if (fips_verify_hmac_signature(hmac_key, hmac_signature))
	{
		DBG1("  integrity test passed");
	}
	else
	{
		DBG1("  integrity test failed");
		status = 3;
		goto end;
	}
#endif /* INTEGRITY_TEST */

	/* load the signer's RSA private key */
	if (keyfile != NULL)
	{
		signerKey = private_key_create_from_file(keyfile, &passphrase);

		if (signerKey == NULL)
		{
			goto end;
		}
	}

	/* load the signer's X.509 certificate */
	if (certfile != NULL)
	{
		signerCert = lib->creds->create(lib->creds,
										CRED_CERTIFICATE, CERT_X509,
										BUILD_FROM_FILE, certfile,
										BUILD_X509_FLAG, 0,
										BUILD_END);
		if (signerCert == NULL)
		{
			goto end;
		}
	}

	/* load the users's X.509 certificate */
	if (usercertfile != NULL)
	{
		userCert = lib->creds->create(lib->creds,
									  CRED_CERTIFICATE, CERT_X509,
									  BUILD_FROM_FILE, usercertfile,
									  BUILD_X509_FLAG, 0,
									  BUILD_END);
		if (userCert == NULL)
		{
			goto end;
		}
	}

	/* compute validity interval */
	validity = (validity)? validity : DEFAULT_VALIDITY;
	notBefore = (notBefore == UNDEFINED_TIME) ? time(NULL) : notBefore;
	notAfter =  (notAfter  == UNDEFINED_TIME) ? time(NULL) + validity : notAfter;

	/* build and parse attribute certificate */
	if (userCert != NULL && signerCert != NULL && signerKey != NULL)
	{
		/* read the serial number and increment it by one */
		serial = read_serial();

		attr_cert = lib->creds->create(lib->creds,
							CRED_CERTIFICATE, CERT_X509_AC,
							BUILD_CERT, userCert,
							BUILD_NOT_BEFORE_TIME, notBefore,
							BUILD_NOT_AFTER_TIME, notAfter,
							BUILD_SERIAL, serial,
							BUILD_IETF_GROUP_ATTR, groups,
							BUILD_SIGNING_CERT, signerCert,
							BUILD_SIGNING_KEY, signerKey,
							BUILD_END);
		if (!attr_cert)
		{
			goto end;
		}
	
		/* write the attribute certificate to file */
		attr_chunk = attr_cert->get_encoding(attr_cert);
		if (chunk_write(attr_chunk, outfile, 0022, TRUE))
		{
			DBG1("  wrote attribute cert file '%s' (%u bytes)", outfile, attr_chunk.len);
			write_serial(serial);
			status = 0;
		}
	}
	else
	{
		usage("some of the mandatory parameters --usercert --cert --key "
			  "are missing");
	}

end:
	/* delete all dynamically allocated objects */
	DESTROY_IF(signerKey);
	DESTROY_IF(signerCert);
	DESTROY_IF(userCert);
	DESTROY_IF(attr_cert);
	free(attr_chunk.ptr);
	free(serial.ptr);
	closelog();
	dbg = dbg_default;
	options->destroy(options);
	library_deinit();
	exit(status);
}
