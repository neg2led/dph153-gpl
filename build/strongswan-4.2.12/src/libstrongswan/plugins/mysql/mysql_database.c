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
 * $Id: mysql_database.c 4193 2008-07-21 11:13:06Z martin $
 */

#define _GNU_SOURCE
#include <string.h>
#include <pthread.h>
#include <mysql/mysql.h>

#include "mysql_database.h"

#include <debug.h>
#include <utils/mutex.h>
#include <utils/linked_list.h>

/* Older mysql.h headers do not define it, but we need it. It is not returned
 * in in MySQL 4 by default, but by MySQL 5. To avoid this problem, we catch
 * it in all cases. */
#ifndef MYSQL_DATA_TRUNCATED
#define MYSQL_DATA_TRUNCATED 101
#endif

typedef struct private_mysql_database_t private_mysql_database_t;

/**
 * private data of mysql_database
 */
struct private_mysql_database_t {

	/**
	 * public functions
	 */
	mysql_database_t public;
	
	/**
	 * connection pool, contains conn_t
	 */
	linked_list_t *pool;
	
	/**
	 * mutex to lock pool
	 */
	mutex_t *mutex;
	
	/**
 	 * hostname to connect to
 	 */
	char *host;
	
	/**
	 * username to use
	 */
	char *username;
	
	/**
	 * password
	 */
	char *password;
	
	/**
	 * database name
	 */
	char *database;
	
	/**
	 * tcp port
	 */
	int port;
};

typedef struct conn_t conn_t;

/**
 * connection pool entry
 */
struct conn_t {
	
	/**
	 * MySQL database connection
	 */
	MYSQL *mysql;
	
	/**
	 * connection in use?
	 */
	bool in_use;
};

/**
 * Release a mysql connection
 */
static void conn_release(conn_t *conn)
{
	conn->in_use = FALSE;
}
/**
 * thread specific initialization flag
 */
pthread_key_t initialized;

/**
 * Initialize a thread for mysql usage
 */
static void thread_initialize()
{
	if (pthread_getspecific(initialized) == NULL)
	{
		pthread_setspecific(initialized, (void*)TRUE);
		mysql_thread_init();
	}
}

/**
 * mysql library initialization function
 */
bool mysql_database_init()
{
	if (mysql_library_init(0, NULL, NULL))
	{
		return FALSE;
	}
	if (pthread_key_create(&initialized, (void*)mysql_thread_end))
	{
		mysql_library_end();
		return FALSE;
	}
	return TRUE;
}

/**
 * mysql library cleanup function
 */
void mysql_database_deinit()
{
	pthread_key_delete(initialized);
	mysql_thread_end();
	/* mysql_library_end(); would be the clean way, however, it hangs... */
}

/**
 * Destroy a mysql connection
 */
static void conn_destroy(conn_t *this)
{
	mysql_close(this->mysql);
	free(this);
}

/**
 * Acquire/Reuse a mysql connection
 */
static conn_t *conn_get(private_mysql_database_t *this)
{
	conn_t *current, *found = NULL;
	enumerator_t *enumerator;
	
	thread_initialize();
	
	while (TRUE)
	{
		this->mutex->lock(this->mutex);
		enumerator = this->pool->create_enumerator(this->pool);
		while (enumerator->enumerate(enumerator, &current))
		{
			if (!current->in_use)
			{
				found = current;
				found->in_use = TRUE;
				break;
			}
		}
		enumerator->destroy(enumerator);
		this->mutex->unlock(this->mutex);
		if (found)
		{	/* check connection if found, release if ping fails */
			if (mysql_ping(found->mysql) == 0)
			{
				break;
			}
			this->mutex->lock(this->mutex);
			this->pool->remove(this->pool, found, NULL);
			this->mutex->unlock(this->mutex);
			conn_destroy(found);
			found = NULL;
			continue;
		}
		break;
	}
	if (found == NULL)
	{
		found = malloc_thing(conn_t);
		found->in_use = TRUE;
		found->mysql = mysql_init(NULL);
		if (!mysql_real_connect(found->mysql, this->host, this->username,
								this->password, this->database, this->port,
								NULL, 0))
		{
			DBG1("connecting to mysql://%s:***@%s:%d/%s failed: %s",
				 this->username, this->host, this->port, this->database,
				 mysql_error(found->mysql));
			conn_destroy(found);
			found = NULL;
		}
		else
		{
			this->mutex->lock(this->mutex);
			this->pool->insert_last(this->pool, found);
			DBG2("increased MySQL connection pool size to %d",
				 this->pool->get_count(this->pool));
			this->mutex->unlock(this->mutex);
		}
	}
	return found;
}

/**
 * Create and run a MySQL stmt using a sql string and args
 */
static MYSQL_STMT* run(MYSQL *mysql, char *sql, va_list *args)
{
	MYSQL_STMT *stmt;
	int params;
	
	stmt = mysql_stmt_init(mysql);
	if (stmt == NULL)
	{
    	DBG1("creating MySQL statement failed: %s", mysql_error(mysql));
		return NULL;
	}
	if (mysql_stmt_prepare(stmt, sql, strlen(sql)))
	{
    	DBG1("preparing MySQL statement failed: %s", mysql_stmt_error(stmt));
    	mysql_stmt_close(stmt);
    	return NULL;
	}
	params = mysql_stmt_param_count(stmt);
	if (params > 0)
	{
		int i;
		MYSQL_BIND *bind;
	
		bind = alloca(sizeof(MYSQL_BIND) * params);
		memset(bind, 0, sizeof(MYSQL_BIND) * params);
		
		for (i = 0; i < params; i++)
		{
			switch (va_arg(*args, db_type_t))
			{
				case DB_INT:
				{
					bind[i].buffer_type = MYSQL_TYPE_LONG;
					bind[i].buffer = (char*)alloca(sizeof(int));
					*(int*)bind[i].buffer = va_arg(*args, int);
					bind[i].buffer_length = sizeof(int);
					break;
				}
				case DB_UINT:
				{
					bind[i].buffer_type = MYSQL_TYPE_LONG;
					bind[i].buffer = (char*)alloca(sizeof(u_int));
					*(u_int*)bind[i].buffer = va_arg(*args, u_int);
					bind[i].buffer_length = sizeof(u_int);
					bind[i].is_unsigned = TRUE;
					break;
				}
				case DB_TEXT:
				{
					bind[i].buffer_type = MYSQL_TYPE_STRING;;
					bind[i].buffer = va_arg(*args, char*);
					if (bind[i].buffer)
					{
						bind[i].buffer_length = strlen(bind[i].buffer);
					}
					break;
				}
				case DB_BLOB:
				{	
					chunk_t chunk = va_arg(*args, chunk_t);
					bind[i].buffer_type = MYSQL_TYPE_BLOB;
					bind[i].buffer = chunk.ptr;
					bind[i].buffer_length = chunk.len;
					break;
				}
				case DB_DOUBLE:
				{
					bind[i].buffer_type = MYSQL_TYPE_DOUBLE;
					bind[i].buffer = (char*)alloca(sizeof(double));
					*(double*)bind[i].buffer = va_arg(*args, double);
					bind[i].buffer_length = sizeof(double);
					break;
				}
    			case DB_NULL:
				{
					bind[i].buffer_type = MYSQL_TYPE_NULL;
					break;
				}
				default:
    				DBG1("invalid data type supplied");
    				mysql_stmt_close(stmt);
    				return NULL;
			}
		}
		if (mysql_stmt_bind_param(stmt, bind))
		{
    		DBG1("binding MySQL param failed: %s", mysql_stmt_error(stmt));
    		mysql_stmt_close(stmt);
			return NULL;
		}
	}
	if (mysql_stmt_execute(stmt))
	{
    	DBG1("executing MySQL statement failed: %s", mysql_stmt_error(stmt));
    	mysql_stmt_close(stmt);
		return NULL;
	}
	return stmt;
}

typedef struct {
	/** implements enumerator_t */
	enumerator_t public;
	/** associated MySQL statement */
	MYSQL_STMT *stmt;
	/** result bindings */
	MYSQL_BIND *bind;
	/** pooled connection handle */
	conn_t *conn;
	/** value for INT, UINT, double */
	union {
		void *p_void;;
		int *p_int;
		u_int *p_uint;
		double *p_double;
	} val;
	/* length for TEXT and BLOB */
	unsigned long *length;
} mysql_enumerator_t;

/**
 * create a mysql enumerator
 */
static void mysql_enumerator_destroy(mysql_enumerator_t *this)
{
	int columns, i;
	
	columns = mysql_stmt_field_count(this->stmt);
	
	for (i = 0; i < columns; i++)
	{
		switch (this->bind[i].buffer_type)
		{
			case MYSQL_TYPE_STRING:
			case MYSQL_TYPE_BLOB:
			{
				free(this->bind[i].buffer);
				break;
			}
			default:
				break;
		}
	}
	mysql_stmt_close(this->stmt);
	conn_release(this->conn);
	free(this->bind);
	free(this->val.p_void);
	free(this->length);
	free(this);
}

/**
 * Implementation of database.query().enumerate
 */
static bool mysql_enumerator_enumerate(mysql_enumerator_t *this, ...)
{
	int i, columns;
	va_list args;
	
	columns = mysql_stmt_field_count(this->stmt);
	
	/* free/reset data set of previous call */
	for (i = 0; i < columns; i++)
	{
		switch (this->bind[i].buffer_type)
		{
			case MYSQL_TYPE_STRING:
			case MYSQL_TYPE_BLOB:
			{
				free(this->bind[i].buffer);
				this->bind[i].buffer = NULL;
				this->bind[i].buffer_length = 0;
				this->bind[i].length = &this->length[i];
				this->length[i] = 0;
				break;
			}
			default:
				break;
		}
	}

	switch (mysql_stmt_fetch(this->stmt))
	{
		case 0:
		case MYSQL_DATA_TRUNCATED:
			break;
		case MYSQL_NO_DATA:
			return FALSE;
		default:
			DBG1("fetching MySQL row failed: %s", mysql_stmt_error(this->stmt));
			return FALSE;
	}
	
	va_start(args, this);
	for (i = 0; i < columns; i++)
	{
		switch (this->bind[i].buffer_type)
		{
			case MYSQL_TYPE_LONG:
			{
				if (this->bind[i].is_unsigned)
				{
					u_int *value = va_arg(args, u_int*);
					*value = this->val.p_uint[i];
				}
				else
				{
					int *value = va_arg(args, int*);
					*value = this->val.p_int[i];
				}
				break;
			}
			case MYSQL_TYPE_STRING:
			{
				char **value = va_arg(args, char**);
				this->bind[i].buffer = malloc(this->length[i]+1);
				this->bind[i].buffer_length = this->length[i];
				*value = this->bind[i].buffer;
	  			mysql_stmt_fetch_column(this->stmt, &this->bind[i], i, 0);
	  			((char*)this->bind[i].buffer)[this->length[i]] = '\0';
	  			break;
			}
			case MYSQL_TYPE_BLOB:
			{
				chunk_t *value = va_arg(args, chunk_t*);
				this->bind[i].buffer = malloc(this->length[i]);
				this->bind[i].buffer_length = this->length[i];
				value->ptr = this->bind[i].buffer;
				value->len = this->length[i];
	  			mysql_stmt_fetch_column(this->stmt, &this->bind[i], i, 0);
	  			break;
			}
			case MYSQL_TYPE_DOUBLE:
			{
				double *value = va_arg(args, double*);
				*value = this->val.p_double[i];
				break;
			}
			default:
				break;
		}
	}
	return TRUE;
}

/**
 * Implementation of database_t.query.
 */
static enumerator_t* query(private_mysql_database_t *this, char *sql, ...)
{
	MYSQL_STMT *stmt;
	va_list args;
	mysql_enumerator_t *enumerator = NULL;
	conn_t *conn;
	
	conn = conn_get(this);
	if (!conn)
	{
		return NULL;
	}

	va_start(args, sql);
	stmt = run(conn->mysql, sql, &args);
	if (stmt)
	{
		int columns, i;
		
		enumerator = malloc_thing(mysql_enumerator_t);
		enumerator->public.enumerate = (void*)mysql_enumerator_enumerate;
		enumerator->public.destroy = (void*)mysql_enumerator_destroy;
		enumerator->stmt = stmt;
		enumerator->conn = conn;
		columns = mysql_stmt_field_count(stmt);
		enumerator->bind = calloc(columns, sizeof(MYSQL_BIND));
		enumerator->length = calloc(columns, sizeof(unsigned long));
		enumerator->val.p_void = calloc(columns, sizeof(enumerator->val));
		for (i = 0; i < columns; i++)
		{
			switch (va_arg(args, db_type_t))
			{
				case DB_INT:
				{
					enumerator->bind[i].buffer_type = MYSQL_TYPE_LONG;
					enumerator->bind[i].buffer = (char*)&enumerator->val.p_int[i];
					break;
				}
				case DB_UINT:
				{
					enumerator->bind[i].buffer_type = MYSQL_TYPE_LONG;
					enumerator->bind[i].buffer = (char*)&enumerator->val.p_uint[i];
					enumerator->bind[i].is_unsigned = TRUE;
					break;
				}
				case DB_TEXT:
				{
					enumerator->bind[i].buffer_type = MYSQL_TYPE_STRING;
					enumerator->bind[i].length = &enumerator->length[i];
					break;
				}
				case DB_BLOB:
				{	
					enumerator->bind[i].buffer_type = MYSQL_TYPE_BLOB;
					enumerator->bind[i].length = &enumerator->length[i];
					break;
				}
				case DB_DOUBLE:
				{
					enumerator->bind[i].buffer_type = MYSQL_TYPE_DOUBLE;
					enumerator->bind[i].buffer = (char*)&enumerator->val.p_double[i];
					break;
				}
				default:
    				DBG1("invalid result data type supplied");
    				mysql_enumerator_destroy(enumerator);
    				va_end(args);
    				return NULL;
			}
		}
		if (mysql_stmt_bind_result(stmt, enumerator->bind))
		{
			DBG1("binding MySQL result failed: %s", mysql_stmt_error(stmt));
    		mysql_enumerator_destroy(enumerator);
    		enumerator = NULL;
		}
	}
	else
	{
		conn_release(conn);
	}
	va_end(args);
	return (enumerator_t*)enumerator;
}

/**
 * Implementation of database_t.execute.
 */
static int execute(private_mysql_database_t *this, int *rowid, char *sql, ...)
{
	MYSQL_STMT *stmt;
	va_list args;
	conn_t *conn;
	int affected = -1;
	
	conn = conn_get(this);
	if (!conn)
	{
		return -1;
	}
	va_start(args, sql);
	stmt = run(conn->mysql, sql, &args);
	if (stmt)
	{
		if (rowid)
		{
			*rowid = mysql_stmt_insert_id(stmt);
		}
		affected = mysql_stmt_affected_rows(stmt);
		mysql_stmt_close(stmt);
	}
	va_end(args);
	conn_release(conn);
	return affected;
}
	
/**
 * Implementation of database_t.get_driver
 */
static db_driver_t get_driver(private_mysql_database_t *this)
{
	return DB_MYSQL;
}

/**
 * Implementation of database_t.destroy
 */
static void destroy(private_mysql_database_t *this)
{
	this->pool->destroy_function(this->pool, (void*)conn_destroy);
	this->mutex->destroy(this->mutex);
	free(this->host);
	free(this->username);
	free(this->password);
	free(this->database);
	free(this);
}

static bool parse_uri(private_mysql_database_t *this, char *uri)
{
	char *username, *password, *host, *port = "0", *database, *pos;

	/**
	 * parse mysql://username:pass@host:port/database uri
	 */
	username = strdupa(uri + 8);
	pos = strchr(username, ':');
	if (pos)
	{
		*pos = '\0';
		password = pos + 1;
		pos = strrchr(password, '@');
		if (pos)
		{
			*pos = '\0';
			host = pos + 1;
			pos = strrchr(host, ':');
			if (pos)
			{
				*pos = '\0';
				port = pos + 1;
				pos = strchr(port, '/');
			}
			else
			{
				pos = strchr(host, '/');
			}
			if (pos)
			{
				*pos = '\0';
				database = pos + 1;
	
				this->host = strdup(host);
				this->username = strdup(username);
				this->password = strdup(password);
				this->database = strdup(database);
				this->port = atoi(port);
				return TRUE;
			}
		}
	}
	DBG1("parsing MySQL database uri '%s' failed", uri);
	return FALSE;
}


/*
 * see header file
 */
mysql_database_t *mysql_database_create(char *uri)
{
	conn_t *conn;
	private_mysql_database_t *this;
	
	if (!strneq(uri, "mysql://", 8))
	{
		return NULL;
	}

	this = malloc_thing(private_mysql_database_t);
	
	this->public.db.query = (enumerator_t* (*)(database_t *this, char *sql, ...))query;
	this->public.db.execute = (int (*)(database_t *this, int *rowid, char *sql, ...))execute;
	this->public.db.get_driver = (db_driver_t(*)(database_t*))get_driver;
	this->public.db.destroy = (void(*)(database_t*))destroy;
	
	if (!parse_uri(this, uri))
	{
		free(this);
		return NULL;
	}
	this->mutex = mutex_create(MUTEX_DEFAULT);
	this->pool = linked_list_create();
	
	/* check connectivity */
	conn = conn_get(this);
	if (!conn)
	{
    	destroy(this);
    	return NULL;
	}
	conn_release(conn);
	return &this->public;
}

