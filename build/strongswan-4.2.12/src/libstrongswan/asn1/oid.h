/* Object identifiers (OIDs) used by strongSwan
 * Copyright (C) 2003-2008 Andreas Steffen, Hochschule fuer Technik Rapperswil
 * 
 * This file has been automatically generated by the script oid.pl
 * Do not edit manually!
 */

#ifndef OID_H_
#define OID_H_

typedef struct {
    u_char octet;
    u_int  next;
    u_int  down;
    const u_char *name;
} oid_t;

extern const oid_t oid_names[];

#define OID_UNKNOWN							-1
#define OID_ROLE							35
#define OID_SUBJECT_KEY_ID					38
#define OID_SUBJECT_ALT_NAME				41
#define OID_BASIC_CONSTRAINTS				43
#define OID_CRL_NUMBER						44
#define OID_CRL_REASON_CODE					45
#define OID_CRL_DISTRIBUTION_POINTS			46
#define OID_AUTHORITY_KEY_ID				48
#define OID_EXTENDED_KEY_USAGE				49
#define OID_TARGET_INFORMATION				50
#define OID_NO_REV_AVAIL					51
#define OID_RSA_ENCRYPTION					65
#define OID_MD2_WITH_RSA					66
#define OID_MD5_WITH_RSA					67
#define OID_SHA1_WITH_RSA					68
#define OID_SHA256_WITH_RSA					69
#define OID_SHA384_WITH_RSA					70
#define OID_SHA512_WITH_RSA					71
#define OID_PKCS7_DATA						73
#define OID_PKCS7_SIGNED_DATA				74
#define OID_PKCS7_ENVELOPED_DATA			75
#define OID_PKCS7_SIGNED_ENVELOPED_DATA		76
#define OID_PKCS7_DIGESTED_DATA				77
#define OID_PKCS7_ENCRYPTED_DATA			78
#define OID_PKCS9_EMAIL						80
#define OID_PKCS9_CONTENT_TYPE				82
#define OID_PKCS9_MESSAGE_DIGEST			83
#define OID_PKCS9_SIGNING_TIME				84
#define OID_MD2								91
#define OID_MD5								92
#define OID_3DES_EDE_CBC					94
#define OID_EC_PUBLICKEY					98
#define OID_C2PNB163V1						101
#define OID_C2PNB163V2						102
#define OID_C2PNB163V3						103
#define OID_C2PNB176W1						104
#define OID_C2PNB191V1						105
#define OID_C2PNB191V2						106
#define OID_C2PNB191V3						107
#define OID_C2PNB191V4						108
#define OID_C2PNB191V5						109
#define OID_C2PNB208W1						110
#define OID_C2PNB239V1						111
#define OID_C2PNB239V2						112
#define OID_C2PNB239V3						113
#define OID_C2PNB239V4						114
#define OID_C2PNB239V5						115
#define OID_C2PNB272W1						116
#define OID_C2PNB304W1						117
#define OID_C2PNB359V1						118
#define OID_C2PNB368W1						119
#define OID_C2PNB431R1						120
#define OID_PRIME192V1						122
#define OID_PRIME192V2						123
#define OID_PRIME192V3						124
#define OID_PRIME239V1						125
#define OID_PRIME239V2						126
#define OID_PRIME239V3						127
#define OID_PRIME256V1						128
#define OID_ECDSA_WITH_SHA1					130
#define OID_AUTHORITY_INFO_ACCESS			156
#define OID_OCSP_SIGNING					166
#define OID_XMPP_ADDR						168
#define OID_AUTHENTICATION_INFO				170
#define OID_ACCESS_IDENTITY					171
#define OID_CHARGING_IDENTITY				172
#define OID_GROUP							173
#define OID_OCSP							175
#define OID_BASIC							176
#define OID_NONCE							177
#define OID_CRL								178
#define OID_RESPONSE						179
#define OID_NO_CHECK						180
#define OID_ARCHIVE_CUTOFF					181
#define OID_SERVICE_LOCATOR					182
#define OID_CA_ISSUERS						183
#define OID_DES_CBC							187
#define OID_SHA1							188
#define OID_SHA1_WITH_RSA_OIW				189
#define OID_SECT163K1						200
#define OID_SECT163R1						201
#define OID_SECT239K1						202
#define OID_SECT113R1						203
#define OID_SECT113R2						204
#define OID_SECT112R1						205
#define OID_SECT112R2						206
#define OID_SECT160R1						207
#define OID_SECT160K1						208
#define OID_SECT256K1						209
#define OID_SECT163R2						210
#define OID_SECT283K1						211
#define OID_SECT283R1						212
#define OID_SECT131R1						213
#define OID_SECT131R2						214
#define OID_SECT193R1						215
#define OID_SECT193R2						216
#define OID_SECT233K1						217
#define OID_SECT233R1						218
#define OID_SECT128R1						219
#define OID_SECT128R2						220
#define OID_SECT160R2						221
#define OID_SECT192K1						222
#define OID_SECT224K1						223
#define OID_SECT224R1						224
#define OID_SECT384R1						225
#define OID_SECT521R1						226
#define OID_SECT409K1						227
#define OID_SECT409R1						228
#define OID_SECT571K1						229
#define OID_SECT571R1						230
#define OID_SHA256							239
#define OID_SHA384							240
#define OID_SHA512							241
#define OID_NS_REVOCATION_URL				247
#define OID_NS_CA_REVOCATION_URL			248
#define OID_NS_CA_POLICY_URL				249
#define OID_NS_COMMENT						250
#define OID_PKI_MESSAGE_TYPE				259
#define OID_PKI_STATUS						260
#define OID_PKI_FAIL_INFO					261
#define OID_PKI_SENDER_NONCE				262
#define OID_PKI_RECIPIENT_NONCE				263
#define OID_PKI_TRANS_ID					264

#endif /* OID_H_ */