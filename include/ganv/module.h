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

#ifndef GANV_MODULE_H
#define GANV_MODULE_H

#include <ganv/box.h>
#include <ganv/canvas.h>
#include <ganv/types.h>

#include <glib-object.h>
#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GANV_TYPE_MODULE            (ganv_module_get_type())
#define GANV_MODULE(obj)            (GTK_CHECK_CAST((obj), GANV_TYPE_MODULE, GanvModule))
#define GANV_MODULE_CLASS(klass)    (GTK_CHECK_CLASS_CAST((klass), GANV_TYPE_MODULE, GanvModuleClass))
#define GANV_IS_MODULE(obj)         (GTK_CHECK_TYPE((obj), GANV_TYPE_MODULE))
#define GANV_IS_MODULE_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GANV_TYPE_MODULE))
#define GANV_MODULE_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS((obj), GANV_TYPE_MODULE, GanvModuleClass))

struct _GanvModuleClass;

typedef struct _GanvModuleClass   GanvModuleClass;
typedef struct _GanvModulePrivate GanvModulePrivate;

typedef void (*GanvPortFunc)(GanvPort* port, void* data);

struct _GanvModule {
	GanvBox            box;
	GanvModulePrivate* impl;
};

struct _GanvModuleClass {
	GanvBoxClass parent_class;

	/* Reserved for future expansion */
	gpointer spare_vmethods[4];
};

GType ganv_module_get_type(void) G_GNUC_CONST;

GanvModule*
ganv_module_new(GanvCanvas* canvas,
                const char* first_prop_name, ...);

guint
ganv_module_num_ports(const GanvModule* module);

/**
 * ganv_module_get_port:
 *
 * Get a port by index.
 *
 * Return value: (transfer none): The port on @module at @index.
 */
GanvPort*
ganv_module_get_port(GanvModule* module,
                     guint       index);

double
ganv_module_get_empty_port_breadth(const GanvModule* module);

double
ganv_module_get_empty_port_depth(const GanvModule* module);

void
ganv_module_embed(GanvModule* module, GtkWidget* widget);

void
ganv_module_set_direction(GanvModule* module, GanvDirection direction);

/**
 * ganv_module_for_each_port:
 * @module: The module.
 * @f: (scope call): A function to call on every port on @module.
 * @data: User data to pass to @f.
 */
void
ganv_module_for_each_port(GanvModule*  module,
                          GanvPortFunc f,
                          void*        data);

G_END_DECLS

#endif  /* GANV_MODULE_H */
