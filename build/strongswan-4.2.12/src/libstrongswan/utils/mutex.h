/*
 * Copyright (C) 2008 Tobias Brunner
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
 * $Id: mutex.h 4697 2008-11-25 19:30:02Z tobias $
 */

/**
 * @defgroup mutex mutex
 * @{ @ingroup utils
 */

#ifndef MUTEX_H_
#define MUTEX_H_

typedef struct mutex_t mutex_t;
typedef struct condvar_t condvar_t;
typedef struct rwlock_t rwlock_t;
typedef enum mutex_type_t mutex_type_t;
typedef enum condvar_type_t condvar_type_t;
typedef enum rwlock_type_t rwlock_type_t;

#include <library.h>

/**
 * Type of mutex.
 */
enum mutex_type_t {
	/** default mutex */
	MUTEX_DEFAULT	= 0,
	/** allow recursive locking of the mutex */
	MUTEX_RECURSIVE	= 1,
};

/**
 * Type of condvar.
 */
enum condvar_type_t {
	/** default condvar */
	CONDVAR_DEFAULT	= 0,
};

/**
 * Type of read-write lock.
 */
enum rwlock_type_t {
	/** default condvar */
	RWLOCK_DEFAULT	= 0,
};

/**
 * Mutex wrapper implements simple, portable and advanced mutex functions.
 */
struct mutex_t {

	/**
	 * Acquire the lock to the mutex.
	 */
	void (*lock)(mutex_t *this);
	
	/**
	 * Release the lock on the mutex.
	 */
	void (*unlock)(mutex_t *this);
			
	/**
     * Destroy a mutex instance.
     */
    void (*destroy)(mutex_t *this);
};

/**
 * Condvar wrapper to use in conjunction with mutex_t.
 */
struct condvar_t {

	/**
	 * Wait on a condvar until it gets signalized.
	 *
	 * @param mutex			mutex to release while waiting
	 */
	void (*wait)(condvar_t *this, mutex_t *mutex);
	
	/**
	 * Wait on a condvar until it gets signalized, or times out.
	 *
	 * @param mutex			mutex to release while waiting
	 * @param timeout		timeout im ms
	 * @return				TRUE if timed out, FALSE otherwise
	 */
	bool (*timed_wait)(condvar_t *this, mutex_t *mutex, u_int timeout);
	
	/**
	 * Wait on a condvar until it gets signalized, or times out.
	 * 
	 * @param mutex			mutex to release while waiting
	 * @param time			absolute time until timeout
	 * @return				TRUE if timed out, FALSE otherwise
	 */
	bool (*timed_wait_abs)(condvar_t *this, mutex_t *mutex, timeval_t timeout);
	
	/**
	 * Wake up a single thread in a condvar.
	 */
	void (*signal)(condvar_t *this);
	
	/**
	 * Wake up all threads in a condvar.
	 */
	void (*broadcast)(condvar_t *this);
	
	/**
	 * Destroy a condvar and free its resources.
	 */
	void (*destroy)(condvar_t *this);
};

/**
 * Read-Write lock wrapper.
 */
struct rwlock_t {

	/**
	 * Acquire the read lock.
	 */
	void (*read_lock)(rwlock_t *this);
	
	/**
	 * Acquire the write lock.
	 */
	void (*write_lock)(rwlock_t *this);
	
	/**
	 * Try to acquire the write lock.
	 *
	 * Never blocks, but returns FALSE if the lock was already occupied.
	 *
	 * @return		TRUE if lock acquired
	 */
	bool (*try_write_lock)(rwlock_t *this);	
	
	/**
	 * Release any acquired lock.
	 */
	void (*unlock)(rwlock_t *this);
	
	/**
	 * Destroy the read-write lock.
	 */
	void (*destroy)(rwlock_t *this);
};

/**
 * Create a mutex instance.
 *
 * @param type		type of mutex to create
 * @return			unlocked mutex instance
 */
mutex_t *mutex_create(mutex_type_t type);

/**
 * Create a condvar instance.
 *
 * @param type		type of condvar to create
 * @return			condvar instance
 */
condvar_t *condvar_create(condvar_type_t type);

/**
 * Create a read-write lock instance.
 *
 * @param type		type of rwlock to create
 * @return			unlocked rwlock instance
 */
rwlock_t *rwlock_create(rwlock_type_t type);

#endif /* MUTEX_H_ @}*/
