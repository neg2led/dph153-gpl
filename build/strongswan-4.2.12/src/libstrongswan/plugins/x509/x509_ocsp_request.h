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
 */

/**
 * @defgroup x509_ocsp_request x509_ocsp_request
 * @{ @ingroup x509_p
 */

#ifndef X509_OCSP_REQUEST_H_
#define X509_OCSP_REQUEST_H_

#include <credentials/certificates/ocsp_request.h>

typedef struct x509_ocsp_request_t x509_ocsp_request_t;

/**
 * Implementation of ocsp_request_t using own ASN1 parser.
 */
struct x509_ocsp_request_t {

	/**
	 * Implements the ocsp_request_t interface
	 */
	ocsp_request_t interface;
};

/**
 * Create the building facility for OCSP requests.
 *
 * The resulting builder accepts:
 * 	BUILD_CA_CERT:		CA of the checked certificates, exactly one
 *	BUILD_CERT:			certificates to check with the request, at least one
 * 	BUILD_SUBJECT:		subject requesting check, optional
 *	BUILD_SIGNING_CERT:	certificate to create requestor signature, optional
 *	BUILD_SIGNING_KEY:	private key to create requestor signature, optional
 *
 * @param type		certificate type, CERT_X509_OCSP_REQUEST only
 * @return			builder instance to build OCSP requests
 */
builder_t *x509_ocsp_request_builder(certificate_type_t type);

#endif /* X509_OCSP_REQUEST_H_ @}*/
