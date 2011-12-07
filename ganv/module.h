/* This file is part of Ganv.
 * Copyright 2007-2011 David Robillard <http://drobilla.net>
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

#include <glib.h>
#include <gtk/gtk.h>

#include "ganv/box.h"

G_BEGIN_DECLS

#define GANV_TYPE_MODULE            (ganv_module_get_type())
#define GANV_MODULE(obj)            (GTK_CHECK_CAST((obj), GANV_TYPE_MODULE, GanvModule))
#define GANV_MODULE_CLASS(klass)    (GTK_CHECK_CLASS_CAST((klass), GANV_TYPE_MODULE, GanvModuleClass))
#define GANV_IS_MODULE(obj)         (GTK_CHECK_TYPE((obj), GANV_TYPE_MODULE))
#define GANV_IS_MODULE_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GANV_TYPE_MODULE))
#define GANV_MODULE_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS((obj), GANV_TYPE_MODULE, GanvModuleClass))

typedef struct _GanvModuleClass GanvModuleClass;

typedef void (*GanvPortFunction)(GanvPort* port, void* data);
	
struct _GanvModule
{
	GanvBox          box;
	GPtrArray*       ports;
	GnomeCanvasItem* icon_box;
	GnomeCanvasItem* embed_item;
	int              embed_width;
	int              embed_height;
	double           widest_input;
	double           widest_output;
	gboolean         show_port_labels;
	gboolean         must_resize;
	gboolean         port_size_changed;
};

struct _GanvModuleClass {
	GanvBoxClass parent_class;
};

GType ganv_module_get_type(void);

GanvModule*
ganv_module_new(GanvCanvas* canvas,
                const char* first_prop_name, ...);

void
ganv_module_add_port(GanvModule* module,
                     GanvPort*   port);

double
ganv_module_get_empty_port_breadth(const GanvModule* module);

double
ganv_module_get_empty_port_depth(const GanvModule* module);

void
ganv_module_set_icon(GanvModule* module,
                     GdkPixbuf*  icon);

void
ganv_module_embed(GanvModule* module,
                  GtkWidget*  widget);

/**
 * ganv_module_for_each_port:
 * @f: (scope call): A function to call on every port on @module.
 */
void
ganv_module_for_each_port(GanvModule*      module,
                          GanvPortFunction f,
                          void*            data);

G_END_DECLS

#endif  /* GANV_MODULE_H */
