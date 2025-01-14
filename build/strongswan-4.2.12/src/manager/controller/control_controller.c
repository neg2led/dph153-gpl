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
 * $Id: control_controller.c 3589 2008-03-13 14:14:44Z martin $
 */

#include "control_controller.h"
#include "../manager.h"
#include "../gateway.h"

#include <xml.h>

#include <library.h>


typedef struct private_control_controller_t private_control_controller_t;

/**
 * private data of the task manager
 */
struct private_control_controller_t {

	/**
	 * public functions
	 */
	control_controller_t public;
	
	/**
	 * manager instance
	 */
	manager_t *manager;
};

/**
 * handle the result of a control operation
 */
static void handle_result(private_control_controller_t *this, request_t *r,
						  enumerator_t *e)
{
	enumerator_t *e1;
	xml_t *xml;
	char *name, *value;
	int num = 0;
	
	if (e)
	{
		while (e->enumerate(e, &xml, &name, &value))
		{
			if (streq(name, "status"))
			{
				if (value && atoi(value) == 0)
				{
					r->set(r, "result", "Operation executed successfully:");
				}
				else
				{
					r->set(r, "result", "Operation failed:");
				}
			}
			else if (streq(name, "log"))
			{
				e1 = xml->children(xml);
				while (e1->enumerate(e1, &xml, &name, &value))
				{
					if (streq(name, "item"))
					{
						r->setf(r, "log.%d=%s", ++num, value);
					}
				}
				e1->destroy(e1);
			}
		}
		e->destroy(e);
		r->render(r, "templates/control/result.cs");
	}
	else
	{
		r->set(r, "title", "Error");
		r->set(r, "error", "controlling the gateway failed");
		r->render(r, "templates/error.cs");
	}
}

/**
 * initiate an IKE or CHILD SA
 */
static void initiate(private_control_controller_t *this, request_t *r,
					 bool ike, char *config)
{
	gateway_t *gateway;
	enumerator_t *e;

	r->setf(r, "title=Establishing %s SA %s", ike ? "IKE" : "CHILD", config);
	gateway = this->manager->select_gateway(this->manager, 0);
	e = gateway->initiate(gateway, ike, config);
	handle_result(this, r, e);
}

/**
 * terminate an IKE or CHILD SA
 */
static void terminate(private_control_controller_t *this, request_t *r,
					  bool ike, u_int32_t id)
{
	gateway_t *gateway;
	enumerator_t *e;
	
	r->setf(r, "title=Terminate %s SA %d", ike ? "IKE" : "CHILD", id);
	gateway = this->manager->select_gateway(this->manager, 0);
	e = gateway->terminate(gateway, ike, id);
	handle_result(this, r, e);
}

/**
 * Implementation of controller_t.get_name
 */
static char* get_name(private_control_controller_t *this)
{
	return "control";
}

/**
 * Implementation of controller_t.handle
 */
static void handle(private_control_controller_t *this,
				   request_t *request, char *action, char *str)
{
	if (!this->manager->logged_in(this->manager))
	{
		return request->redirect(request, "auth/login");
	}
	if (this->manager->select_gateway(this->manager, 0) == NULL)
	{
		return request->redirect(request, "gateway/list");
	}
	if (action)
	{
		u_int32_t id;
	
		if (streq(action, "terminateike"))
		{
			if (str && (id = atoi(str)))
			{
				return terminate(this, request, TRUE, id);
			}
		}
		if (streq(action, "terminatechild"))
		{
			if (str && (id = atoi(str)))
			{
				return terminate(this, request, FALSE, id);
			}
		}
		if (streq(action, "initiateike"))
		{
			if (str)
			{
				return initiate(this, request, TRUE, str);
			}
		}
		if (streq(action, "initiatechild"))
		{
			if (str)
			{
				return initiate(this, request, FALSE, str);
			}
		}
	}
	return request->redirect(request, "ikesa/list");
}

/**
 * Implementation of controller_t.destroy
 */
static void destroy(private_control_controller_t *this)
{
	free(this);
}

/*
 * see header file
 */
controller_t *control_controller_create(context_t *context, void *param)
{
	private_control_controller_t *this = malloc_thing(private_control_controller_t);

	this->public.controller.get_name = (char*(*)(controller_t*))get_name;
	this->public.controller.handle = (void(*)(controller_t*,request_t*,char*,char*,char*,char*,char*))handle;
	this->public.controller.destroy = (void(*)(controller_t*))destroy;
	
	this->manager = (manager_t*)context;
	
	return &this->public.controller;
}

