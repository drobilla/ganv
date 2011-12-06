/* This file is part of Ganv.
 * Copyright 2007-2011 David Robillard <http://drobilla.net>
 *
 * Ganv is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Ganv is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Ganv.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GANV_PORT_H
#define GANV_PORT_H

#include "ganv/box.h"

struct _GanvModule;

G_BEGIN_DECLS

#define GANV_TYPE_PORT            (ganv_port_get_type())
#define GANV_PORT(obj)            (GTK_CHECK_CAST((obj), GANV_TYPE_PORT, GanvPort))
#define GANV_PORT_CLASS(klass)    (GTK_CHECK_CLASS_CAST((klass), GANV_TYPE_PORT, GanvPortClass))
#define GANV_IS_PORT(obj)         (GTK_CHECK_TYPE((obj), GANV_TYPE_PORT))
#define GANV_IS_PORT_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GANV_TYPE_PORT))
#define GANV_PORT_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS((obj), GANV_TYPE_PORT, GanvPortClass))

typedef struct _GanvPortClass GanvPortClass;

typedef struct {
	GanvBox* rect;
	float          value;
	float          min;
	float          max;
	gboolean       is_toggle;
} GanvPortControl;

struct _GanvPort
{
	GanvBox          box;
	GanvPortControl* control;
	gboolean               is_input;
};

struct _GanvPortClass {
	GanvBoxClass parent_class;
};

GType ganv_port_get_type(void);

GanvPort*
ganv_port_new(GanvModule* module,
                     gboolean          is_input,
                     const char*       first_prop_name, ...);

void
ganv_port_show_control(GanvPort* port);

void
ganv_port_hide_control(GanvPort* port);

void
ganv_port_set_control_is_toggle(GanvPort* port,
                                       gboolean        is_toggle);

void
ganv_port_set_control_value(GanvPort* port,
                                   float           value);

void
ganv_port_set_control_min(GanvPort* port,
                                 float           min);

void
ganv_port_set_control_max(GanvPort* port,
                                 float           max);

double
ganv_port_get_natural_width(const GanvPort* port);

/**
 * ganv_port_get_module:
 * Return value: (transfer none): The module @a port is on.
 */
GanvModule*
ganv_port_get_module(const GanvPort* port);

static inline float
ganv_port_get_control_value(const GanvPort* port)
{
	return port->control ? port->control->value : 0.0f;
}

static inline float
ganv_port_get_control_min(const GanvPort* port)
{
	return port->control ? port->control->min : 0.0f;
}

static inline float
ganv_port_get_control_max(const GanvPort* port)
{
	return port->control ? port->control->max : 0.0f;
}

static inline gboolean
ganv_port_is_input(const GanvPort* port)
{
	return port->is_input;
}

static inline gboolean
ganv_port_is_output(const GanvPort* port)
{
	return !port->is_input;
}
	
G_END_DECLS

#endif  /* GANV_PORT_H */
