/*
 * Copyright (C) 2006 Martin Will
 * Copyright (C) 2000-2008 Andreas Steffen
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
 * $Id: asn1_parser.c 3894 2008-04-28 18:44:21Z andreas $
 */

#include <stdio.h>
#include <string.h>
#include <time.h>

#include <library.h>
#include <debug.h>

#include "asn1.h"
#include "asn1_parser.h"

#define ASN1_MAX_LEVEL	10

typedef struct private_asn1_parser_t private_asn1_parser_t;

/**
 * Private data of an asn1_cxt_t object.
 */
struct private_asn1_parser_t {
	/**
	 * Public interface.
	 */
	asn1_parser_t public;

	/**
	 * Syntax definition of ASN.1 object
	 */
	asn1Object_t const *objects;

	/**
	 * Current syntax definition line
	 */
	int line;

	/**
	 * Current stat of the parsing operation
	 */
	bool success;

	/**
	 * Declare object data as private - use debug level 4 to log it 
	 */
	bool private;

	/**
	 * Top-most type is implicit - ignore it
	 */
	bool implicit;

	/**
	 * Top-most parsing level - defaults to 0
	 */
	u_int level0;

	/**
	 * Jump back address for loops for each level
	 */
	int loopAddr[ASN1_MAX_LEVEL + 1];

	/**
	 * Current parsing pointer for each level
	 */
	chunk_t blobs[ASN1_MAX_LEVEL + 2];
};

/**
 * Implementation of asn1_parser_t.iterate
 */
static bool iterate(private_asn1_parser_t *this, int *objectID, chunk_t *object)
{
	chunk_t *blob, *blob1;
	u_char *start_ptr;
	u_int level;
	asn1Object_t obj;
	
	*object = chunk_empty;

	/* Advance to the next object syntax definition line */
	obj = this->objects[++(this->line)];

	/* Terminate if the end of the object syntax definition has been reached */
	if (obj.flags & ASN1_EXIT)
	{
		return FALSE;
	}
		
	if (obj.flags & ASN1_END)  /* end of loop or option found */
	{
		if (this->loopAddr[obj.level] && this->blobs[obj.level+1].len > 0)
		{
			this->line = this->loopAddr[obj.level]; /* another iteration */
			obj = this->objects[this->line];
		}
		else
		{
			this->loopAddr[obj.level] = 0;         /* exit loop or option*/
			goto end;
		}
	}
	
	level = this->level0 + obj.level;
	blob = this->blobs + obj.level;
	blob1 = blob + 1;
	start_ptr = blob->ptr;
	
	/* handle ASN.1 defaults values */
	if ((obj.flags & ASN1_DEF) && (blob->len == 0 || *start_ptr != obj.type) )
	{
		/* field is missing */
		DBG2("L%d - %s:", level, obj.name);
		if (obj.type & ASN1_CONSTRUCTED)
		{
			this->line++ ;  /* skip context-specific tag */
		}
		goto end;
	}
	
	/* handle ASN.1 options */
	
	if ((obj.flags & ASN1_OPT)
			&& (blob->len == 0 || *start_ptr != obj.type))
	{
		/* advance to end of missing option field */
		do
		{
			this->line++;
		}
		while (!((this->objects[this->line].flags & ASN1_END) &&
				 (this->objects[this->line].level == obj.level)));
		goto end;
	}
		
	/* an ASN.1 object must possess at least a tag and length field */
	
	if (blob->len < 2)
	{
		DBG1("L%d - %s:  ASN.1 object smaller than 2 octets",
					level, obj.name);
		this->success = FALSE;
		goto end;
	}
	
	blob1->len = asn1_length(blob);
	
	if (blob1->len == ASN1_INVALID_LENGTH || blob->len < blob1->len)
	{
		DBG1("L%d - %s:  length of ASN.1 object invalid or too large", 
					level, obj.name);
		this->success = FALSE;
	}
	
	blob1->ptr = blob->ptr;
	blob->ptr += blob1->len;
	blob->len -= blob1->len;
	
	/* return raw ASN.1 object without prior type checking */
	
	if (obj.flags & ASN1_RAW)
	{
		DBG2("L%d - %s:", level, obj.name);
		object->ptr = start_ptr;
		object->len = (size_t)(blob->ptr - start_ptr);
		goto end;
	}

	if (*start_ptr != obj.type && !(this->implicit && this->line == 0))
	{
		DBG1("L%d - %s: ASN1 tag 0x%02x expected, but is 0x%02x",
					level, obj.name, obj.type, *start_ptr);
		DBG3("%b", start_ptr, (u_int)(blob->ptr - start_ptr));
		this->success = FALSE;
		goto end;
	}
	
	DBG2("L%d - %s:", level, obj.name);
	
	/* In case of "SEQUENCE OF" or "SET OF" start a loop */	
	if (obj.flags & ASN1_LOOP)
	{
		if (blob1->len > 0)
		{
			/* at least one item, start the loop */
			this->loopAddr[obj.level] = this->line + 1;
		}
		else
		{
			/* no items, advance directly to end of loop */
			do
			{
				this->line++;
			}
			while (!((this->objects[this->line].flags & ASN1_END) &&
					 (this->objects[this->line].level == obj.level)));
			goto end;
		}
	}

	if (obj.flags & ASN1_OBJ)
	{
		object->ptr = start_ptr;
		object->len = (size_t)(blob->ptr - start_ptr);
		if (this->private)
		{
			DBG4("%B", object);
		}
		else
		{
			DBG3("%B", object);
		}
	}
	else if (obj.flags & ASN1_BODY)
	{
		*object = *blob1;
		asn1_debug_simple_object(*object, obj.type, this->private);
	}

end:
	*objectID = this->line;
	return this->success;
}

/**
 * Implementation of asn1_parser_t.get_level
 */
static u_int get_level(private_asn1_parser_t *this)
{
	return this->level0 + this->objects[this->line].level;
}

/**
 * Implementation of asn1_parser_t.set_top_level
 */
static void set_top_level(private_asn1_parser_t *this, u_int level0)
{
	this->level0 = level0;
}

/**
 * Implementation of asn1_parser_t.set_flags
 */
static void set_flags(private_asn1_parser_t *this, bool implicit, bool private)
{
	this->implicit = implicit;
	this->private = private;
}

/**
 * Implementation of asn1_parser_t.success
 */
static bool success(private_asn1_parser_t *this)
{
	return this->success;
}

/**
 * Implementation of asn1_parser_t.destroy
 */
static void destroy(private_asn1_parser_t *this)
{
	free(this);
}

/**
 * Defined in header.
 */
asn1_parser_t* asn1_parser_create(asn1Object_t const *objects, chunk_t blob)
{
	private_asn1_parser_t *this = malloc_thing(private_asn1_parser_t);

	memset(this, '\0', sizeof(private_asn1_parser_t));
	this->objects = objects;
	this->blobs[0] = blob;
	this->line = -1;
	this->success = TRUE;

	this->public.iterate = (bool (*)(asn1_parser_t*, int*, chunk_t*))iterate;
	this->public.get_level = (u_int (*)(asn1_parser_t*))get_level;
	this->public.set_top_level = (void (*)(asn1_parser_t*, u_int))set_top_level;
	this->public.set_flags = (void (*)(asn1_parser_t*, bool, bool))set_flags;
	this->public.success = (bool (*)(asn1_parser_t*))success;
	this->public.destroy = (void (*)(asn1_parser_t*))destroy;

	return &this->public;
}
