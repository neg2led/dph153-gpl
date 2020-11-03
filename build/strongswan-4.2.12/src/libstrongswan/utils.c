/*
 * Copyright (C) 2008 Tobias Brunner
 * Copyright (C) 2005-2008 Martin Willi
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
 * $Id: utils.c 4742 2008-12-03 09:45:58Z tobias $
 */

#include "utils.h"

#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>

#include <enum.h>
#include <debug.h>

ENUM(status_names, SUCCESS, DESTROY_ME,
	"SUCCESS",
	"FAILED",
	"OUT_OF_RES",
	"ALREADY_DONE",
	"NOT_SUPPORTED",
	"INVALID_ARG",
	"NOT_FOUND",
	"PARSE_ERROR",
	"VERIFY_ERROR",
	"INVALID_STATE",
	"DESTROY_ME",
	"NEED_MORE",
);

/**
 * Described in header.
 */
void *clalloc(void * pointer, size_t size)
{
	void *data;
	data = malloc(size);
	
	memcpy(data, pointer, size);
	
	return (data);
}

/**
 * Described in header.
 */
void memxor(u_int8_t dest[], u_int8_t src[], size_t n)
{
	int i = 0, m;
	
	m = n - sizeof(long);
	while (i < m)
	{
		*(long*)(dest + i) ^= *(long*)(src + i);
		i += sizeof(long);
	}
	while (i < n)
	{
		dest[i] ^= src[i];
		i++;
	}
}

/**
 * Described in header.
 */
void *memstr(const void *haystack, const char *needle, size_t n)
{
	unsigned const char *pos = haystack;
	size_t l = strlen(needle);
	for (; n >= l; ++pos, --n)
	{
		if (memeq(pos, needle, l))
		{
			return (void*)pos;
		}
	}
	return NULL;
}

/**
 * Described in header.
 */
bool mkdir_p(const char *path, mode_t mode)
{
	size_t len;
	char *pos, full[PATH_MAX];
	pos = full;
	if (!path || *path == '\0')
	{
		return TRUE;
	}
	len = snprintf(full, sizeof(full)-1, "%s", path);
	if (len < 0 || len >= sizeof(full)-1)
	{
		DBG1("path string %s too long", path);
		return FALSE;
	}
	/* ensure that the path ends with a '/' */
	if (full[len-1] != '/')
	{
		full[len++] = '/';
		full[len] = '\0';
	}
	/* skip '/' at the beginning */
	while (*pos == '/')
	{
		pos++;
	}
	while ((pos = strchr(pos, '/')))
	{
		*pos = '\0';
		if (access(full, F_OK) < 0)
		{
			if (mkdir(full, mode) < 0)
			{
				DBG1("failed to create directory %s", full);
				return FALSE;
			}
		}
		*pos = '/';
		pos++;
	}
	return TRUE;
}

/**
 * return null
 */
void *return_null()
{
	return NULL;
}

/**
 * nop operation
 */
void nop()
{
}

#ifndef HAVE_GCC_ATOMIC_OPERATIONS
#include <pthread.h>

/**
 * We use a single mutex for all refcount variables. 
 */
static pthread_mutex_t ref_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * Increase refcount
 */
void ref_get(refcount_t *ref)
{
	pthread_mutex_lock(&ref_mutex);
	(*ref)++;
	pthread_mutex_unlock(&ref_mutex);
}

/**
 * Decrease refcount
 */
bool ref_put(refcount_t *ref)
{
	bool more_refs;
	
	pthread_mutex_lock(&ref_mutex);
	more_refs = --(*ref);
	pthread_mutex_unlock(&ref_mutex);
	return !more_refs;
}
#endif /* HAVE_GCC_ATOMIC_OPERATIONS */

/**
 * output handler in printf() for time_t
 */
static int time_print(FILE *stream, const struct printf_info *info,
					  const void *const *args)
{
	static const char* months[] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};
	time_t *time = *((time_t**)(args[0]));
	bool utc = TRUE;
	struct tm t;
	
	if (info->alt)
	{
		utc = *((bool*)(args[1]));
	}
	if (time == UNDEFINED_TIME)
	{
		return fprintf(stream, "--- -- --:--:--%s----",
					   info->alt ? " UTC " : " ");
	}
	if (utc)
	{
		gmtime_r(time, &t);
	}
	else
	{
		localtime_r(time, &t);
	}
	return fprintf(stream, "%s %02d %02d:%02d:%02d%s%04d",
				   months[t.tm_mon], t.tm_mday, t.tm_hour, t.tm_min,
				   t.tm_sec, utc ? " UTC " : " ", t.tm_year + 1900);
}

/**
 * arginfo handler for printf() time
 */
static int time_arginfo(const struct printf_info *info, size_t n, int *argtypes)
{
	if (info->alt)
	{
		if (n > 1)
		{
			argtypes[0] = PA_POINTER;
			argtypes[1] = PA_INT;
		}
		return 2;
	}
	
	if (n > 0)
	{
		argtypes[0] = PA_POINTER;
	}
	return 1;
}

/**
 * output handler in printf() for time deltas
 */
static int time_delta_print(FILE *stream, const struct printf_info *info,
							const void *const *args)
{
	char* unit = "second";
	time_t *arg1, *arg2;
	time_t delta;
	
	arg1 = *((time_t**)(args[0]));
	if (info->alt)
	{
		arg2 = *((time_t**)(args[1]));
		delta = abs(*arg1 - *arg2);
	}
	else
	{
		delta = *arg1;
	}

	if (delta > 2 * 60 * 60 * 24)
	{
		delta /= 60 * 60 * 24;
		unit = "day";
	}
	else if (delta > 2 * 60 * 60)
	{
		delta /= 60 * 60;
		unit = "hour";
	}
	else if (delta > 2 * 60)
	{
		delta /= 60;
		unit = "minute";
	}
	return fprintf(stream, "%d %s%s", delta, unit, (delta == 1)? "":"s");
}

/**
 * arginfo handler for printf() time deltas
 */
int time_delta_arginfo(const struct printf_info *info, size_t n, int *argtypes)
{
	if (info->alt)
	{
		if (n > 1)
		{
			argtypes[0] = PA_POINTER;
			argtypes[1] = PA_POINTER;
		}
		return 2;
	}
	
	if (n > 0)
	{
		argtypes[0] = PA_POINTER;
	}
	return 1;
}

/**
 * Number of bytes per line to dump raw data
 */
#define BYTES_PER_LINE 16

static char hexdig_upper[] = "0123456789ABCDEF";

/**
 * output handler in printf() for mem ranges
 */
static int mem_print(FILE *stream, const struct printf_info *info,
					 const void *const *args)
{
	char *bytes = *((void**)(args[0]));
	int len = *((size_t*)(args[1]));
	
	char buffer[BYTES_PER_LINE * 3];
	char ascii_buffer[BYTES_PER_LINE + 1];
	char *buffer_pos = buffer;
	char *bytes_pos  = bytes;
	char *bytes_roof = bytes + len;
	int line_start = 0;
	int i = 0;
	int written = 0;
	
	written += fprintf(stream, "=> %d bytes @ %p", len, bytes);
	
	while (bytes_pos < bytes_roof)
	{
		*buffer_pos++ = hexdig_upper[(*bytes_pos >> 4) & 0xF];
		*buffer_pos++ = hexdig_upper[ *bytes_pos       & 0xF];

		ascii_buffer[i++] =
				(*bytes_pos > 31 && *bytes_pos < 127) ? *bytes_pos : '.';

		if (++bytes_pos == bytes_roof || i == BYTES_PER_LINE) 
		{
			int padding = 3 * (BYTES_PER_LINE - i);
			int written;
			
			while (padding--)
			{
				*buffer_pos++ = ' ';
			}
			*buffer_pos++ = '\0';
			ascii_buffer[i] = '\0';
			
			written += fprintf(stream, "\n%4d: %s  %s",
							   line_start, buffer, ascii_buffer);

			
			buffer_pos = buffer;
			line_start += BYTES_PER_LINE;
			i = 0;
		}
		else
		{
			*buffer_pos++ = ' ';
		}
	}
	return written;
}

/**
 * arginfo handler for printf() mem ranges
 */
int mem_arginfo(const struct printf_info *info, size_t n, int *argtypes)
{
	if (n > 1)
	{
		argtypes[0] = PA_POINTER;
		argtypes[1] = PA_INT;
	}
	return 2;
}

/**
 * return printf hook functions for a time
 */
printf_hook_functions_t time_get_printf_hooks()
{
	printf_hook_functions_t hooks = {time_print, time_arginfo};
	
	return hooks;
}

/**
 * return printf hook functions for a time delta
 */
printf_hook_functions_t time_delta_get_printf_hooks()
{
	printf_hook_functions_t hooks = {time_delta_print, time_delta_arginfo};
	
	return hooks;
}

/**
 * return printf hook functions for mem ranges
 */
printf_hook_functions_t mem_get_printf_hooks()
{
	printf_hook_functions_t hooks = {mem_print, mem_arginfo};
	
	return hooks;
}

