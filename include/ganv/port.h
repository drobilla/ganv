/* This file is part of Ganv.
 * Copyright 2007-2014 David Robillard <http://drobilla.net>
 *
 * Ganv is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or any later version.
 *
 * Ganv is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 *
 * You should have received a copy of the GNU General Public License along
 * with Ganv.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GANV_PORT_H
#define GANV_PORT_H

#include <ganv/box.h>
#include <ganv/types.h>

#include <glib-object.h>
#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GANV_TYPE_PORT            (ganv_port_get_type())
#define GANV_PORT(obj)            (GTK_CHECK_CAST((obj), GANV_TYPE_PORT, GanvPort))
#define GANV_PORT_CLASS(klass)    (GTK_CHECK_CLASS_CAST((klass), GANV_TYPE_PORT, GanvPortClass))
#define GANV_IS_PORT(obj)         (GTK_CHECK_TYPE((obj), GANV_TYPE_PORT))
#define GANV_IS_PORT_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GANV_TYPE_PORT))
#define GANV_PORT_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS((obj), GANV_TYPE_PORT, GanvPortClass))

struct _GanvPortClass;

typedef struct _GanvPortClass   GanvPortClass;
typedef struct _GanvPortPrivate GanvPortPrivate;

struct _GanvPort {
	GanvBox          box;
	GanvPortPrivate* impl;
};

struct _GanvPortClass {
	GanvBoxClass parent_class;

	/* Reserved for future expansion */
	gpointer spare_vmethods[4];
};

GType ganv_port_get_type(void) G_GNUC_CONST;

GanvPort*
ganv_port_new(GanvModule* module,
              gboolean    is_input,
              const char* first_prop_name, ...);

void
ganv_port_set_value_label(GanvPort*   port,
                          const char* str);

void
ganv_port_show_control(GanvPort* port);

void
ganv_port_hide_control(GanvPort* port);

void
ganv_port_set_control_is_toggle(GanvPort* port,
                                gboolean  is_toggle);

void
ganv_port_set_control_is_integer(GanvPort* port,
                                 gboolean  is_integer);

void
ganv_port_set_control_value(GanvPort* port,
                            float     value);

void
ganv_port_set_control_min(GanvPort* port,
                          float     min);

void
ganv_port_set_control_max(GanvPort* port,
                          float     max);

double
ganv_port_get_natural_width(const GanvPort* port);

/**
 * ganv_port_get_module:
 * @port: The port.
 *
 * Return value: (transfer none): The module @port is on.
 */
GanvModule* ganv_port_get_module(const GanvPort* port);

float    ganv_port_get_control_value(const GanvPort* port);
float    ganv_port_get_control_min(const GanvPort* port);
float    ganv_port_get_control_max(const GanvPort* port);
gboolean ganv_port_is_input(const GanvPort* port);
gboolean ganv_port_is_output(const GanvPort* port);

G_END_DECLS

#endif  /* GANV_PORT_H */
