/* General-purpose hooks.
   Copyright (C) 2002, 2003, 2004, 2005, 2007 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 3, or (at your option) any
   later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING3.  If not see
   <http://www.gnu.org/licenses/>.  

   In other words, you are welcome to use, share and improve this program.
   You are forbidden to forbid anyone else to use, share and improve
   what you give them.   Help stamp out software-hoarding!  */

#ifndef GCC_HOOKS_H
#define GCC_HOOKS_H

#include "machmode.h"

extern bool hook_bool_void_false (void);
extern bool hook_bool_void_true (void);
extern bool hook_bool_bool_false (bool);
extern bool hook_bool_mode_false (enum machine_mode);
extern bool hook_bool_mode_const_rtx_false (enum machine_mode, const_rtx);
extern bool hook_bool_mode_const_rtx_true (enum machine_mode, const_rtx);
extern bool hook_bool_tree_false (tree);
extern bool hook_bool_const_tree_false (const_tree);
extern bool hook_bool_tree_true (tree);
extern bool hook_bool_const_tree_true (const_tree);
extern bool hook_bool_const_tree_hwi_hwi_const_tree_false (const_tree,
							   HOST_WIDE_INT,
							   HOST_WIDE_INT,
							   const_tree);
extern bool hook_bool_const_tree_hwi_hwi_const_tree_true (const_tree,
							  HOST_WIDE_INT,
							  HOST_WIDE_INT,
							  const_tree);
extern bool hook_bool_rtx_false (rtx);
extern bool hook_bool_uintp_uintp_false (unsigned int *, unsigned int *);
extern bool hook_bool_rtx_int_int_intp_false (rtx, int, int, int *);
extern bool hook_bool_constcharptr_size_t_false (const char *, size_t);
extern bool hook_bool_size_t_constcharptr_int_true (size_t, const char *, int);
extern bool hook_bool_tree_tree_false (tree, tree);
extern bool hook_bool_tree_bool_false (tree, bool);

extern void hook_void_void (void);
extern void hook_intp_void (int *);
extern void hook_void_constcharptr (const char *);
extern void hook_void_FILEptr_constcharptr (FILE *, const char *);
extern void hook_void_tree (tree);
extern void hook_void_tree_treeptr (tree, tree *);

extern int hook_int_const_tree_0 (const_tree);
extern int hook_int_const_tree_const_tree_1 (const_tree, const_tree);
extern int hook_int_rtx_0 (rtx);
extern int hook_int_size_t_constcharptr_int_0 (size_t, const char *, int);
extern int hook_int_void_no_regs (void);

extern tree hook_tree_const_tree_null (const_tree);

extern tree hook_tree_tree_tree_null (tree, tree);
extern tree hook_tree_tree_tree_tree_3rd_identity (tree, tree, tree);
extern tree hook_tree_tree_tree_bool_null (tree, tree, bool);

extern unsigned hook_uint_uint_constcharptrptr_0 (unsigned, const char **);

extern bool default_can_output_mi_thunk_no_vcall (const_tree, HOST_WIDE_INT,
						  HOST_WIDE_INT, const_tree);

extern rtx hook_rtx_rtx_identity (rtx);
extern rtx hook_rtx_rtx_null (rtx);
extern rtx hook_rtx_tree_int_null (tree, int);

extern const char *hook_constcharptr_const_tree_null (const_tree);
extern const char *hook_constcharptr_const_rtx_null (const_rtx);
extern const char *hook_constcharptr_const_tree_const_tree_null (const_tree, const_tree);
extern const char *hook_constcharptr_int_const_tree_null (int, const_tree);
extern const char *hook_constcharptr_int_const_tree_const_tree_null (int, const_tree, const_tree);
#endif
