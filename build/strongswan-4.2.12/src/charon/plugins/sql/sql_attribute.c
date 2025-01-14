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
 *
 * $Id$
 */

#include "sql_attribute.h"

#include <time.h>

#include <daemon.h>

typedef struct private_sql_attribute_t private_sql_attribute_t;

/**
 * private data of sql_attribute
 */
struct private_sql_attribute_t {

	/**
	 * public functions
	 */
	sql_attribute_t public;
	
	/**
	 * database connection
	 */
	database_t *db;
	
	/**
	 * wheter to record lease history in lease table
	 */
	bool history;
};

/**
 * lookup/insert an identity
 */
static u_int get_identity(private_sql_attribute_t *this, identification_t *id)
{
	enumerator_t *e;
	u_int row;
	
	/* look for peer identity in the identities table */
	e = this->db->query(this->db,
						"SELECT id FROM identities WHERE type = ? AND data = ?",
						DB_INT, id->get_type(id), DB_BLOB, id->get_encoding(id),
						DB_UINT);
						
	if (e && e->enumerate(e, &row))
	{
		e->destroy(e);
		return row;
	}
	DESTROY_IF(e);
	/* not found, insert new one */
	if (this->db->execute(this->db, &row,
				  "INSERT INTO identities (type, data) VALUES (?, ?)",
				  DB_INT, id->get_type(id), DB_BLOB, id->get_encoding(id)) == 1)
	{
		return row;
	}
	return 0;
}

/**
 * Lookup pool by name
 */
static u_int get_pool(private_sql_attribute_t *this, char *name, u_int *timeout)
{
	enumerator_t *e;
	u_int pool;

	e = this->db->query(this->db, "SELECT id, timeout FROM pools WHERE name = ?",
						DB_TEXT, name, DB_UINT, DB_UINT);
	if (e && e->enumerate(e, &pool, timeout))
	{
		e->destroy(e);
		return pool;
	}
	DBG1(DBG_CFG, "ip pool '%s' not found");
	return 0;
}

/**
 * Lookup a lease
 */
static host_t *get_address(private_sql_attribute_t *this, char *name,
						   u_int pool, u_int timeout, u_int identity)
{
	enumerator_t *e;
	u_int id;
	chunk_t address;
	host_t *host;
	time_t now = time(NULL);
	
	/* We check for leases for that identity first and for other expired
	 * leases afterwards. We select an address as a candidate, but double
	 * check if it is still valid in the update. This allows us to work
	 * without locking. */
	
	/* check for an existing lease for that identity  */
	while (TRUE)
	{
		e = this->db->query(this->db,
				"SELECT id, address FROM addresses "
				"WHERE pool = ? AND identity = ? AND released != 0 LIMIT 1",
				DB_UINT, pool, DB_UINT, identity, DB_UINT, DB_BLOB);
		if (!e || !e->enumerate(e, &id, &address))
		{
			DESTROY_IF(e);
			break;	
		}
		address = chunk_clonea(address);
		e->destroy(e);
		if (this->db->execute(this->db, NULL,
				"UPDATE addresses SET acquired = ?, released = 0 "
				"WHERE id = ? AND identity = ? AND released != 0",
				DB_UINT, now, DB_UINT, id, DB_UINT, identity) > 0)
		{
			host = host_create_from_chunk(AF_UNSPEC, address, 0);
			if (host)
			{
				DBG1(DBG_CFG, "acquired existing lease "
					 "for address %H in pool '%s'", host, name);
				return host;
			}
		}
	}
	
	/* check for an expired lease */
	while (TRUE)
	{
		e = this->db->query(this->db,
				"SELECT id, address FROM addresses "
				"WHERE pool = ? AND released != 0 AND released < ? LIMIT 1",
				DB_UINT, pool, DB_UINT, now - timeout, DB_UINT, DB_BLOB);
		if (!e || !e->enumerate(e, &id, &address))
		{
			DESTROY_IF(e);
			break;	
		}
		address = chunk_clonea(address);
		e->destroy(e);
			
		if (this->db->execute(this->db, NULL,
				"UPDATE addresses SET "
				"acquired = ?, released = 0, identity = ? "
				"WHERE id = ? AND released != 0 AND released < ?",
				DB_UINT, now, DB_UINT, identity,
				DB_UINT, id, DB_UINT, now - timeout) > 0)
		{
			host = host_create_from_chunk(AF_UNSPEC, address, 0);
			if (host)
			{
				DBG1(DBG_CFG, "acquired new lease "
					 "for address %H in pool '%s'", host, name);
				return host;
			}
		}
	}
	DBG1(DBG_CFG, "no available address found in pool '%s'", name);
	return 0;
}

/**
 * Implementation of attribute_provider_t.acquire_address
 */
static host_t* acquire_address(private_sql_attribute_t *this,
							   char *name, identification_t *id,
							   auth_info_t *auth, host_t *requested)
{
	enumerator_t *enumerator;
	u_int pool, timeout, identity;
	host_t *address = NULL;
	
	identity = get_identity(this, id);
	if (identity)
	{
		enumerator = enumerator_create_token(name, ",", " ");
		while (enumerator->enumerate(enumerator, &name))
		{
			pool = get_pool(this, name, &timeout);
			if (pool)
			{
				address = get_address(this, name, pool, timeout, identity);
				if (address)
				{
					break;
				}
			}
		}
		enumerator->destroy(enumerator);
	}
	return address;
}

/**
 * Implementation of attribute_provider_t.release_address
 */
static bool release_address(private_sql_attribute_t *this,
							char *name, host_t *address, identification_t *id)
{
	enumerator_t *enumerator;
	bool found = FALSE;
	time_t now = time(NULL);
	
	enumerator = enumerator_create_token(name, ",", " ");
	while (enumerator->enumerate(enumerator, &name))
	{
		u_int pool, timeout;
		
		pool = get_pool(this, name, &timeout);
		if (pool)
		{
			if (this->history)
			{
				this->db->execute(this->db, NULL,
					"INSERT INTO leases (address, identity, acquired, released)"
					" SELECT id, identity, acquired, ? FROM addresses "
					" WHERE pool = ? AND address = ?",
					DB_UINT, now, DB_UINT, pool,
					DB_BLOB, address->get_address(address));
			}
			if (this->db->execute(this->db, NULL,
					"UPDATE addresses SET released = ? WHERE "
					"pool = ? AND address = ?", DB_UINT, time(NULL),
					DB_UINT, pool, DB_BLOB, address->get_address(address)) > 0)
			{
				found = TRUE;
				break;
			}
		}
	}
	enumerator->destroy(enumerator);
	return found;
}

/**
 * Implementation of sql_attribute_t.destroy
 */
static void destroy(private_sql_attribute_t *this)
{
	free(this);
}

/*
 * see header file
 */
sql_attribute_t *sql_attribute_create(database_t *db)
{
	private_sql_attribute_t *this = malloc_thing(private_sql_attribute_t);
	time_t now = time(NULL);
	
	this->public.provider.acquire_address = (host_t*(*)(attribute_provider_t *this, char*, identification_t *,auth_info_t *, host_t *))acquire_address;
	this->public.provider.release_address = (bool(*)(attribute_provider_t *this, char*,host_t *, identification_t*))release_address;
	this->public.destroy = (void(*)(sql_attribute_t*))destroy;
	
	this->db = db;
	this->history = lib->settings->get_bool(lib->settings,
									"charon.plugins.sql.lease_history", TRUE);
	
	/* close any "online" leases in the case we crashed */
	if (this->history)
	{
		this->db->execute(this->db, NULL,
					"INSERT INTO leases (address, identity, acquired, released)"
					" SELECT id, identity, acquired, ? FROM addresses "
					" WHERE released = 0", DB_UINT, now);
	}
	this->db->execute(this->db, NULL,
					  "UPDATE addresses SET released = ? WHERE released = 0",
					  DB_UINT, now);
	return &this->public;
}

