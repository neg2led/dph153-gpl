/*
 * Special support for eabi and SVR4
 *
 *   Copyright (C) 1995, 1996, 1998, 2000, 2001, 2008
 *   Free Software Foundation, Inc.
 *   Written By Michael Meissner
 *   64-bit support written by David Edelsohn
 * 
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 * 
 * In addition to the permissions in the GNU General Public License, the
 * Free Software Foundation gives you unlimited permission to link the
 * compiled version of this file with other programs, and to distribute
 * those programs without any restriction coming from the use of this
 * file.  (The General Public License restrictions do apply in other
 * respects; for example, they cover modification of the file, and
 * distribution when not linked into another program.)
 * 
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 * 
 *    As a special exception, if you link this library with files
 *    compiled with GCC to produce an executable, this does not cause
 *    the resulting executable to be covered by the GNU General Public License.
 *    This exception does not however invalidate any other reasons why
 *    the executable file might be covered by the GNU General Public License.
 */ 

/* Do any initializations needed for the eabi environment */

	.file	"crtresgpr.asm"
	.section ".text"
	#include "ppc-asm.h"

/* On PowerPC64 Linux, these functions are provided by the linker.  */
#ifndef __powerpc64__

/* Routines for restoring integer registers, called by the compiler.  */
/* Called with r11 pointing to the stack header word of the caller of the */
/* function, just beyond the end of the integer restore area.  */

HIDDEN_FUNC(_restgpr_14)	lwz	14,-72(11)	/* restore gp registers */
HIDDEN_FUNC(_restgpr_15)	lwz	15,-68(11)
HIDDEN_FUNC(_restgpr_16)	lwz	16,-64(11)
HIDDEN_FUNC(_restgpr_17)	lwz	17,-60(11)
HIDDEN_FUNC(_restgpr_18)	lwz	18,-56(11)
HIDDEN_FUNC(_restgpr_19)	lwz	19,-52(11)
HIDDEN_FUNC(_restgpr_20)	lwz	20,-48(11)
HIDDEN_FUNC(_restgpr_21)	lwz	21,-44(11)
HIDDEN_FUNC(_restgpr_22)	lwz	22,-40(11)
HIDDEN_FUNC(_restgpr_23)	lwz	23,-36(11)
HIDDEN_FUNC(_restgpr_24)	lwz	24,-32(11)
HIDDEN_FUNC(_restgpr_25)	lwz	25,-28(11)
HIDDEN_FUNC(_restgpr_26)	lwz	26,-24(11)
HIDDEN_FUNC(_restgpr_27)	lwz	27,-20(11)
HIDDEN_FUNC(_restgpr_28)	lwz	28,-16(11)
HIDDEN_FUNC(_restgpr_29)	lwz	29,-12(11)
HIDDEN_FUNC(_restgpr_30)	lwz	30,-8(11)
HIDDEN_FUNC(_restgpr_31)	lwz	31,-4(11)
			blr
FUNC_END(_restgpr_31)
FUNC_END(_restgpr_30)
FUNC_END(_restgpr_29)
FUNC_END(_restgpr_28)
FUNC_END(_restgpr_27)
FUNC_END(_restgpr_26)
FUNC_END(_restgpr_25)
FUNC_END(_restgpr_24)
FUNC_END(_restgpr_23)
FUNC_END(_restgpr_22)
FUNC_END(_restgpr_21)
FUNC_END(_restgpr_20)
FUNC_END(_restgpr_19)
FUNC_END(_restgpr_18)
FUNC_END(_restgpr_17)
FUNC_END(_restgpr_16)
FUNC_END(_restgpr_15)
FUNC_END(_restgpr_14)

#endif
