/*
 * Copyright (C) 2005-2008 Martin Willi
 * Copyright (C) 2006-2007 Tobias Brunner
 * Copyright (C) 2006 Daniel Roethlisberger
 * Copyright (C) 2005 Jan Hutter
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
 * @defgroup host host
 * @{ @ingroup utils
 */

#ifndef HOST_H_
#define HOST_H_

typedef enum host_diff_t host_diff_t;
typedef struct host_t host_t;

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <library.h>

/**
 * Differences between two hosts. They differ in
 * address, port, or both.
 */
enum host_diff_t {
	HOST_DIFF_NONE = 0,
	HOST_DIFF_ADDR = 1,
	HOST_DIFF_PORT = 2,
};

/**
 * Representates a Host
 * 
 * Host object, identifies a address:port pair and defines some 
 * useful functions on it.
 */
struct host_t {
	
	/** 
	 * Build a clone of this host object.
	 * 
	 * @return		cloned host
	 */
	host_t *(*clone) (host_t *this);
	
	/** 
	 * Get a pointer to the internal sockaddr struct.
	 * 
	 * This is used for sending and receiving via sockets.
	 * 
	 * @return		pointer to the internal sockaddr structure
	 */
	sockaddr_t  *(*get_sockaddr) (host_t *this);
	
	/** 
	 * Get the length of the sockaddr struct.
	 * 
	 * Depending on the family, the length of the sockaddr struct
	 * is different. Use this function to get the length of the sockaddr
	 * struct returned by get_sock_addr.
	 * 
	 * This is used for sending and receiving via sockets.
	 * 
	 * @return		length of the sockaddr struct
	 */
	socklen_t *(*get_sockaddr_len) (host_t *this);
	
	/**
	 * Gets the family of the address
	 * 
	 * @return		family
	 */
	int (*get_family) (host_t *this);
	
	/** 
	 * Checks if the ip address of host is set to default route.
	 * 
	 * @return		TRUE if host is 0.0.0.0 or 0::0, FALSE otherwise
	 */
	bool (*is_anyaddr) (host_t *this);
	
	/** 
	 * Get the address of this host as chunk_t
	 * 
	 * Returned chunk points to internal data.
	 * 
	 * @return		address string, 
	 */
	chunk_t (*get_address) (host_t *this);
		
	/** 
	 * Get the port of this host
	 * 
	 * @return		port number
	 */
	u_int16_t (*get_port) (host_t *this);

	/** 
	 * Set the port of this host
	 *
	 * @param port	port numer
	 */
	void (*set_port) (host_t *this, u_int16_t port);
		
	/** 
	 * Compare the ips of two hosts hosts.
	 * 
	 * @param other	the other to compare
	 * @return		TRUE if addresses are equal.
	 */
	bool (*ip_equals) (host_t *this, host_t *other);
		
	/** 
	 * Compare two hosts, with port.
	 * 
	 * @param other	the other to compare
	 * @return		TRUE if addresses and ports are equal.
	 */
	bool (*equals) (host_t *this, host_t *other);

	/** 
	 * Compare two hosts and return the differences.
	 *
	 * @param other	the other to compare
	 * @return		differences in a combination of host_diff_t's
	 */
	host_diff_t (*get_differences) (host_t *this, host_t *other);
	
	/** 
	 * Destroy this host object.
	 */
	void (*destroy) (host_t *this);
};

/**
 * Constructor to create a host_t object from an address string.
 *
 * @param string		string of an address, such as "152.96.193.130"
 * @param port			port number
 * @return 				host_t, NULL if string not an address.
 */
host_t *host_create_from_string(char *string, u_int16_t port);

/**
 * Constructor to create a host_t from a DNS name.
 *
 * @param string		hostname to resolve
 * @param family		family to prefer, 0 for first match
 * @param port			port number
 * @return 				host_t, NULL lookup failed
 */
host_t *host_create_from_dns(char *string, int family, u_int16_t port);

/**
 * Constructor to create a host_t object from an address chunk.
 *
 * If family is AF_UNSPEC, it is guessed using address.len.
 *
 * @param family 		Address family, such as AF_INET or AF_INET6
 * @param address		address as chunk_t in network order
 * @param port			port number
 * @return 				host_t, NULL if family not supported/chunk invalid
 */
host_t *host_create_from_chunk(int family, chunk_t address, u_int16_t port);

/**
 * Constructor to create a host_t object from a sockaddr struct
 *
 * @param sockaddr		sockaddr struct which contains family, address and port
 * @return 				host_t, NULL if family not supported
 */
host_t *host_create_from_sockaddr(sockaddr_t *sockaddr);

/**
 * Create a host without an address, a "any" host.
 *
 * @param family		family of the any host
 * @return 				host_t, NULL if family not supported
 */
host_t *host_create_any(int family);

/**
 * Get printf hooks for a host.
 *
 * Arguments are: 
 *    host_t *host
 * Use #-modifier to include port number
 */
printf_hook_functions_t host_get_printf_hooks();

#endif /* HOST_H_ @}*/
