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
 * @defgroup databasei database
 * @{ @ingroup database
 */

#ifndef DATABASE_H_
#define DATABASE_H_

typedef enum db_type_t db_type_t;
typedef enum db_driver_t db_driver_t;
typedef struct database_t database_t;

#include <utils/enumerator.h>

/**
 * Database column types
 */
enum db_type_t {
	/** integer type, argument is an "int" */
	DB_INT,
	/** unsigned integer, argument is an "u_int" */
	DB_UINT,
	/** string type, argument is a "char*" */
	DB_TEXT,
	/** binary large object type, argument is a "chunk_t" */
	DB_BLOB,
	/** floating point, argument is a "double" */
	DB_DOUBLE,
	/** NULL, takes no argument */
	DB_NULL,
};

/**
 * Database implementation type.
 */
enum db_driver_t {
	/** SQLite database */
	DB_SQLITE,
	/** MySQL database */
	DB_MYSQL,
};

/**
 * Interface for a database implementation.
 *
 * @code
   int affected, rowid, aint;
   char *atext;
   database_t *db;
   enumerator_t *enumerator;
   
   db = lib->database->create("mysql://user:pass@host/database");
   affected = db->execute(db, &rowid, "INSERT INTO table VALUES (?, ?)",
   						  DB_INT, 77, DB_TEXT, "a text");
   printf("inserted %d row, new row ID: %d\n", affected, rowid);
   
   enumerator = db->query(db, "SELECT aint, atext FROM table WHERE aint > ?",
   						  DB_INT, 10, 		// 1 argument to SQL string
   						  DB_INT, DB_TEXT); // 2 enumerated types in query
   if (enumerator)
   {
       while (enumerator->enumerate(enumerator, &aint, &atext))
       {
           printf("%d: %s\n", aint, atext);
       }
       enumerator->destroy(enumerator);
   }
   @endcode
 */
struct database_t {
	
	/**
	 * Run a query which returns rows, such as a SELECT.
	 *
	 * @param sql		sql query string, containing '?' placeholders
	 * @param ...		list of sql placeholder db_type_t followed by its value,
	 *                  followed by enumerators arguments as db_type_t's
	 * @return			enumerator as defined with arguments, NULL on failure
	 */
	enumerator_t* (*query)(database_t *this, char *sql, ...);
	
	/**
	 * Execute a query which dows not return rows, such as INSERT.
	 *
	 * @param rowid		pointer to write inserted AUTO_INCREMENT row ID, or NULL
	 * @param sql		sql string, containing '?' placeholders
	 * @param ...		list of sql placeholder db_type_t followed by its value
	 * @return			number of affected rows, < 0 on failure
	 */
	int (*execute)(database_t *this, int *rowid, char *sql, ...);
	
	/**
	 * Get the database implementation type.
	 *
	 * To allow driver specific SQL or performance optimizations each database
	 * implementations can be queried for its type.
	 *
	 * @return			database implementation type
	 */
	db_driver_t (*get_driver)(database_t *this);
	
	/**
     * Destroy a database connection.
     */
    void (*destroy)(database_t *this);
};

#endif /* DATABASE_H_ @}*/
