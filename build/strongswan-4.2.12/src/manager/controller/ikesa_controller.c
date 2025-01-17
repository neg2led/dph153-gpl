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
 * $Id: ikesa_controller.c 3589 2008-03-13 14:14:44Z martin $
 */

#include "ikesa_controller.h"
#include "../manager.h"
#include "../gateway.h"

#include <xml.h>

#include <library.h>


typedef struct private_ikesa_controller_t private_ikesa_controller_t;

/**
 * private data of the task manager
 */
struct private_ikesa_controller_t {

	/**
	 * public functions
	 */
	ikesa_controller_t public;
	
	/**
	 * manager instance
	 */
	manager_t *manager;
};

/**
 * read XML of a childsa element and fill template
 */
static void process_childsa(private_ikesa_controller_t *this, char *id,
							enumerator_t *e, request_t *r)
{
	xml_t *xml;
	enumerator_t *e1, *e2;
	char *name, *value, *reqid = "", *section = "";
	int num = 0;
	
	while (e->enumerate(e, &xml, &name, &value))
	{
		if (streq(name, "reqid"))
		{
			reqid = value;
		}
		else if (streq(name, "local") || streq(name, "remote"))
		{
			section = name;
			e1 = xml->children(xml);
			while (e1->enumerate(e1, &xml, &name, &value))
			{
				if (streq(name, "networks"))
				{
					e2 = xml->children(xml);
					while (e2->enumerate(e2, &xml, &name, &value))
					{
						if (streq(name, "network"))
						{
							r->setf(r, "ikesas.%s.childsas.%s.%s.networks.%d=%s",
									id, reqid, section, ++num, value);
						}
					}
					e2->destroy(e2);
				}
				else
				{
					r->setf(r, "ikesas.%s.childsas.%s.%s.%s=%s",
							id, reqid, section, name, value);
				}
			}
			e1->destroy(e1);
		}
		else
		{
			r->setf(r, "ikesas.%s.childsas.%s.%s=%s",
					id, reqid, name, value);
		}
	}
}

/**
 * read XML of a ikesa element and fill template
 */
static void process_ikesa(private_ikesa_controller_t *this,
						  enumerator_t *e, request_t *r)
{
	xml_t *xml;
	enumerator_t *e1, *e2;
	char *name, *value, *id = "", *section = "";

	while (e->enumerate(e, &xml, &name, &value))
	{
		if (streq(name, "id"))
		{
			id = value;	
		}
		else if (streq(name, "local") || streq(name, "remote"))
		{
			section = name;
			e1 = xml->children(xml);
			while (e1->enumerate(e1, &xml, &name, &value))
			{
				r->setf(r, "ikesas.%s.%s.%s=%s", id, section, name, value);
			}
			e1->destroy(e1);
		}
		else if (streq(name, "childsalist"))
		{
			e1 = xml->children(xml);
			while (e1->enumerate(e1, &xml, &name, &value))
			{
				if (streq(name, "childsa"))
				{
					e2 = xml->children(xml);
					process_childsa(this, id, e2, r);
					e2->destroy(e2);
				}
			}
			e1->destroy(e1);
		}
		else
		{
			r->setf(r, "ikesas.%s.%s=%s", id, name, value);
		}
	}
}

static void list(private_ikesa_controller_t *this, request_t *r)
{
	gateway_t *gateway;
	xml_t *xml;
	enumerator_t *e1, *e2;
	char *name, *value;

	gateway = this->manager->select_gateway(this->manager, 0);
	e1 = gateway->query_ikesalist(gateway);
	if (e1 == NULL)
	{
		r->set(r, "title", "Error");
		r->set(r, "error", "querying the gateway failed");
		r->render(r, "templates/error.cs");
	}
	else
	{
		r->set(r, "title", "IKE SA overview");

		while (e1->enumerate(e1, &xml, &name, &value))
		{
			if (streq(name, "ikesa"))
			{
				e2 = xml->children(xml);
				process_ikesa(this, e2, r);
				e2->destroy(e2);
			}
		}
		e1->destroy(e1);

		r->render(r, "templates/ikesa/list.cs");
	}
}

/**
 * Implementation of controller_t.get_name
 */
static char* get_name(private_ikesa_controller_t *this)
{
	return "ikesa";
}

/**
 * Implementation of controller_t.handle
 */
static void handle(private_ikesa_controller_t *this,
				   request_t *request, char *action)
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
		if (streq(action, "list"))
		{
			return list(this, request);
		}
	}
	return request->redirect(request, "ikesa/list");
}

/**
 * Implementation of controller_t.destroy
 */
static void destroy(private_ikesa_controller_t *this)
{
	free(this);
}

/*
 * see header file
 */
controller_t *ikesa_controller_create(context_t *context, void *param)
{
	private_ikesa_controller_t *this = malloc_thing(private_ikesa_controller_t);

	this->public.controller.get_name = (char*(*)(controller_t*))get_name;
	this->public.controller.handle = (void(*)(controller_t*,request_t*,char*,char*,char*,char*,char*))handle;
	this->public.controller.destroy = (void(*)(controller_t*))destroy;
	
	this->manager = (manager_t*)context;
	
	return &this->public.controller;
}

