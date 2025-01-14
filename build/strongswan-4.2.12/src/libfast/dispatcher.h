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
 * $Id: dispatcher.h 3519 2008-03-02 12:14:34Z martin $
 */

/**
 * @defgroup libfast libfast
 * @{
 * FastCGI Application Server w/ templates.
 *
 * Libfast is a framework to write web applications in an MVC fashion. It uses
 * the ClearSilver template engine and communicates through FastCGI with
 * the webserver. It is multithreaded and really fast.
 *
 * The application has a global context and a session context. The global
 * context is accessed from all sessions simultaneously and therefore 
 * needs to be threadsave. Often a database wrapper is the global context.
 * The session context is instanciated per session. Sessions are managed
 * automatically through session cookies. The session context is kept alive
 * until the session times out. It must implement the context_t interface and
 * a #context_constructor_t is needed to create instances. To each session, 
 * a set of controllers gets instanciated. The controller instances are per 
 * session, so you can hold private data for each user.
 * Controllers need to implement the controller_t interface and need a 
 * #controller_constructor_t function to create instances.
 *
 * A small example shows how to set up libfast:
 * @code
	dispatcher_t *dispatcher;
	your_global_context_implementation_t *global;
 
 	global = initialize_your_global_context();
 
 	dispatcher = dispatcher_create(NULL, FALSE, 180,
			(context_constructor_t)your_session_context_create, global);
	dispatcher->add_controller(dispatcher, your_controller1_create, param1);
	dispatcher->add_controller(dispatcher, your_controller2_create, param2);
	
	dispatcher->run(dispatcher, 20);
	
	dispatcher->waitsignal(dispatcher);
	
	dispatcher->destroy(dispatcher);
	global->destroy();
   @endcode
 * @}
 *
 * @defgroup dispatcher dispatcher
 * @{ @ingroup libfast
 */

#ifndef DISPATCHER_H_
#define DISPATCHER_H_

#include "controller.h"
#include "filter.h"

typedef struct dispatcher_t dispatcher_t;

/**
 * Dispatcher, accepts connections using multiple threads.
 *
 * The dispatcher creates a session for each client (using SID cookies). In
 * each session, a session context is created using the context constructor.
 * Each controller is instanciated in the session using the controller
 * constructor added with add_controller.
 */
struct dispatcher_t {
	
	/**
	 * Register a controller to the dispatcher.
	 *
	 * The first controller added serves as default controller. Client's
	 * get redirected to it if no other controller matches.
	 *
	 * @param constructor	constructor function to the conntroller
	 * @param param			param to pass to constructor
	 */
	void (*add_controller)(dispatcher_t *this,
						   controller_constructor_t constructor, void *param);

	/**
	 * @brief Add a filter to the dispatcher.
	 *
	 * @param constructor	constructor to create filter in session
	 * @param param			param to pass to constructor
	 */
	void (*add_filter)(dispatcher_t *this,
					   filter_constructor_t constructor, void *param); 	
	
	/**
	 * Start with dispatching.
	 *
	 * Instanciate a constant thread pool and start dispatching requests.
	 *
	 * @param threads		number of dispatching threads
	 */
	void (*run)(dispatcher_t *this, int threads);
	
	/**
	 * Wait for a relevant signal action.
	 *
	 */
	void (*waitsignal)(dispatcher_t *this);
	
	/**
	 * Destroy the dispatcher_t.
	 */
	void (*destroy) (dispatcher_t *this);
};

/**
 * Create a dispatcher.
 *
 * The context constructor is invoked to create a session context for
 * each session.
 *
 * @param socket		FastCGI socket path, NULL for dynamic
 * @param debug			no stripping, no compression, timing information
 * @param timeout		session timeout
 * @param constructor	construction function for session context
 * @param param			parameter to supply to context constructor
 */
dispatcher_t *dispatcher_create(char *socket, bool debug, int timeout,
								context_constructor_t constructor, void *param);

#endif /* DISPATCHER_H_ @} */
