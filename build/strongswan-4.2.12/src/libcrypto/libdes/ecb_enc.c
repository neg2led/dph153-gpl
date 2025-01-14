/* crypto/des/ecb_enc.c */
/* Copyright (C) 1995-1997 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 * 
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 * 
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from 
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 * 
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#include "des_locl.h"
#include "spr.h"

char *libdes_version="libdes v 3.24 - 20-Apr-1996 - eay";
char *DES_version="DES part of SSLeay 0.8.2b 08-Jan-1998";

/* RCSID $Id: ecb_enc.c,v 1.1 2004/03/15 20:35:25 as Exp $ */
/* This function ifdef'ed out for FreeS/WAN project. */
#ifdef notdef
char *des_options()
	{
	static int init=1;
	static char buf[32];

	if (init)
		{
		char *ptr,*unroll,*risc,*size;

		init=0;
#ifdef DES_PTR
		ptr="ptr";
#else
		ptr="idx";
#endif
#if defined(DES_RISC1) || defined(DES_RISC2)
#ifdef DES_RISC1
		risc="risc1";
#endif
#ifdef DES_RISC2
		risc="risc2";
#endif
#else
		risc="cisc";
#endif
#ifdef DES_UNROLL
		unroll="16";
#else
		unroll="4";
#endif
		if (sizeof(DES_LONG) != sizeof(long))
			size="int";
		else
			size="long";
		sprintf(buf,"des(%s,%s,%s,%s)",ptr,risc,unroll,size);
		}
	return(buf);
	}
#endif
		

void des_ecb_encrypt(input, output, ks, enc)
des_cblock (*input);
des_cblock (*output);
des_key_schedule ks;
int enc;
	{
	register DES_LONG l;
	register unsigned char *in,*out;
	DES_LONG ll[2];

	in=(unsigned char *)input;
	out=(unsigned char *)output;
	c2l(in,l); ll[0]=l;
	c2l(in,l); ll[1]=l;
	des_encrypt(ll,ks,enc);
	l=ll[0]; l2c(l,out);
	l=ll[1]; l2c(l,out);
	l=ll[0]=ll[1]=0;
	}

