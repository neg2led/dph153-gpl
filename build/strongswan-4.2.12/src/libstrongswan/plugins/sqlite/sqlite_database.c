/*
 * Copyright (C) 2007 Martin Willi
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
 * $Id: sqlite_database.c 4268 2008-08-21 11:58:58Z andreas $
 */

#include "sqlite_database.h"

#include <sqlite3.h>
#include <unistd.h>
#include <library.h>
#include <debug.h>
#include <utils/mutex.h>

typedef struct private_sqlite_database_t private_sqlite_database_t;

/**
 * private data of sqlite_database
 */
struct private_sqlite_database_t {

	/**
	 * public functions
	 */
	sqlite_database_t public;
	
	/**
	 * sqlite database connection
	 */
	sqlite3 *db;
	
	/**
	 * mutex used to lock execute()
	 */
	mutex_t *mutex;
};

/**
 * Create and run a sqlite stmt using a sql string and args
 */
static sqlite3_stmt* run(private_sqlite_database_t *this, char *sql,
						 va_list *args)
{
	sqlite3_stmt *stmt = NULL;
	int params, i, res = SQLITE_OK;

#ifdef HAVE_SQLITE3_PREPARE_V2
	if (sqlite3_prepare_v2(this->db, sql, -1, &stmt, NULL) == SQLITE_OK)
#else
	if (sqlite3_prepare(this->db, sql, -1, &stmt, NULL) == SQLITE_OK)
#endif
	{
		params = sqlite3_bind_parameter_count(stmt);
		for (i = 1; i <= params; i++)
		{
			switch (va_arg(*args, db_type_t))
			{
				case DB_INT:
				{
					res = sqlite3_bind_int(stmt, i, va_arg(*args, int));
					break;
				}
				case DB_UINT:
				{
					res = sqlite3_bind_int64(stmt, i, va_arg(*args, u_int));
					break;
				}
				case DB_TEXT:
				{
					const char *text = va_arg(*args, const char*);
					res = sqlite3_bind_text(stmt, i, text, -1, SQLITE_STATIC);
					break;
				}
				case DB_BLOB:
				{
					chunk_t c = va_arg(*args, chunk_t);
					res = sqlite3_bind_blob(stmt, i, c.ptr, c.len, SQLITE_STATIC);
					break;
				}
				case DB_DOUBLE:
				{
					res = sqlite3_bind_double(stmt, i, va_arg(*args, double));
					break;
				}
				case DB_NULL:
				{
					res = sqlite3_bind_null(stmt, i);
					break;
				}
				default:
				{
					res = SQLITE_MISUSE;
					break;
				}
			}
			if (res != SQLITE_OK)
			{
				break;
			}
		}
	}
	else
	{
		DBG1("preparing sqlite statement failed: %s", sqlite3_errmsg(this->db));
	}
	if (res != SQLITE_OK)
	{
		DBG1("binding sqlite statement failed: %s", sqlite3_errmsg(this->db));
		sqlite3_finalize(stmt);
		return NULL;
	}
	return stmt;
}

typedef struct {
	/** implements enumerator_t */
	enumerator_t public;
	/** associated sqlite statement */
	sqlite3_stmt *stmt;
	/** number of result columns */
	int count;
	/** column types */
	db_type_t *columns;
	/** back reference to parent */
	private_sqlite_database_t *database;
} sqlite_enumerator_t;

/**
 * destroy a sqlite enumerator
 */
static void sqlite_enumerator_destroy(sqlite_enumerator_t *this)
{
	sqlite3_finalize(this->stmt);
#if SQLITE_VERSION_NUMBER < 3005000
	this->database->mutex->unlock(this->database->mutex);
#endif
	free(this->columns);
	free(this);
}

/**
 * Implementation of database.query().enumerate
 */
static bool sqlite_enumerator_enumerate(sqlite_enumerator_t *this, ...)
{
	int i;
	va_list args;

	switch (sqlite3_step(this->stmt))
	{
		case SQLITE_ROW:
			break;
		default:
			DBG1("stepping sqlite statement failed: %s",
				 sqlite3_errmsg(this->database->db));
			/* fall */
		case SQLITE_DONE:
			return FALSE;
	}
	va_start(args, this);
	for (i = 0; i < this->count; i++)
	{
		switch (this->columns[i])
		{
			case DB_INT:
			{
				int *value = va_arg(args, int*);
				*value = sqlite3_column_int(this->stmt, i);
				break;
			}
			case DB_UINT:
			{
				u_int *value = va_arg(args, u_int*);
				*value = (u_int)sqlite3_column_int64(this->stmt, i);
				break;
			}
			case DB_TEXT:
			{
				const unsigned char **value = va_arg(args, const unsigned char**);
				*value = sqlite3_column_text(this->stmt, i);
				break;
			}
			case DB_BLOB:
			{
				chunk_t *chunk = va_arg(args, chunk_t*);
				chunk->len = sqlite3_column_bytes(this->stmt, i);
				chunk->ptr = (u_char*)sqlite3_column_blob(this->stmt, i);
				break;
			}
			case DB_DOUBLE:
			{
				double *value = va_arg(args, double*);
				*value = sqlite3_column_double(this->stmt, i);
				break;
			}
			default:
				DBG1("invalid result type supplied");
				return FALSE;
		}
	}
	va_end(args);
	return TRUE;
}

/**
 * Implementation of database_t.query.
 */
static enumerator_t* query(private_sqlite_database_t *this, char *sql, ...)
{
	sqlite3_stmt *stmt;
	va_list args;
	sqlite_enumerator_t *enumerator = NULL;
	int i;
	
#if SQLITE_VERSION_NUMBER < 3005000
	/* sqlite connections prior to 3.5 may be used by a single thread only, */
	this->mutex->lock(this->mutex);
#endif
	
	va_start(args, sql);
	stmt = run(this, sql, &args);
	if (stmt)
	{
		enumerator = malloc_thing(sqlite_enumerator_t);
		enumerator->public.enumerate = (void*)sqlite_enumerator_enumerate;
		enumerator->public.destroy = (void*)sqlite_enumerator_destroy;
		enumerator->stmt = stmt;
		enumerator->count = sqlite3_column_count(stmt);
		enumerator->columns = malloc(sizeof(db_type_t) * enumerator->count);
		enumerator->database = this;
		for (i = 0; i < enumerator->count; i++)
		{
			enumerator->columns[i] = va_arg(args, db_type_t);
		}
	}
	va_end(args);
	return (enumerator_t*)enumerator;
}

/**
 * Implementation of database_t.execute.
 */
static int execute(private_sqlite_database_t *this, int *rowid, char *sql, ...)
{
	sqlite3_stmt *stmt;
	int affected = -1;
	va_list args;
	
	/* we need a lock to get our rowid/changes correctly */
	this->mutex->lock(this->mutex);
	va_start(args, sql);
	stmt = run(this, sql, &args);
	va_end(args);
	if (stmt)
	{
		if (sqlite3_step(stmt) == SQLITE_DONE)
		{
			if (rowid)
			{
				*rowid = sqlite3_last_insert_rowid(this->db);
			}
			affected = sqlite3_changes(this->db);
		}
		else
		{
			DBG1("sqlite execute failed: %s", sqlite3_errmsg(this->db));
		}
		sqlite3_finalize(stmt);
	}
	this->mutex->unlock(this->mutex);
	return affected;
}

/**
 * Implementation of database_t.get_driver
 */
static db_driver_t get_driver(private_sqlite_database_t *this)
{
	return DB_SQLITE;
}

/**
 * Busy handler implementation
 */
static int busy_handler(private_sqlite_database_t *this, int count)
{
	/* add a backoff time, quadratically increasing with every try */
	usleep(count * count * 1000);
	/* always retry */
	return 1;
}

/**
 * Implementation of database_t.destroy
 */
static void destroy(private_sqlite_database_t *this)
{
	sqlite3_close(this->db);
	this->mutex->destroy(this->mutex);
	free(this);
}

/*
 * see header file
 */
sqlite_database_t *sqlite_database_create(char *uri)
{
	char *file;
	private_sqlite_database_t *this;
	
	/**
	 * parse sqlite:///path/to/file.db uri
	 */
	if (!strneq(uri, "sqlite://", 9))
	{
		return NULL;
	}
	file = uri + 9;
	
	this = malloc_thing(private_sqlite_database_t);
	
	this->public.db.query = (enumerator_t* (*)(database_t *this, char *sql, ...))query;
	this->public.db.execute = (int (*)(database_t *this, int *rowid, char *sql, ...))execute;
	this->public.db.get_driver = (db_driver_t(*)(database_t*))get_driver;
	this->public.db.destroy = (void(*)(database_t*))destroy;
	
	this->mutex = mutex_create(MUTEX_RECURSIVE);
	
	if (sqlite3_open(file, &this->db) != SQLITE_OK)
	{
		DBG1("opening SQLite database '%s' failed: %s",
			 file, sqlite3_errmsg(this->db));
		destroy(this);
		return NULL;
	}
	
	sqlite3_busy_handler(this->db, (void*)busy_handler, this);
	
	return &this->public;
}

