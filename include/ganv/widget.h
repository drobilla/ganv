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

// IWYU pragma: no_include "ganv-private.h"

/* Based on GnomeCanvasWidget, by Federico Mena <federico@nuclecu.unam.mx>
 * Copyright 1997-2000 Free Software Foundation
 */

#ifndef GANV_WIDGET_H
#define GANV_WIDGET_H

#include <ganv/item.h>

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

#define GANV_TYPE_WIDGET            (ganv_widget_get_type ())
#define GANV_WIDGET(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GANV_TYPE_WIDGET, GanvWidget))
#define GANV_WIDGET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GANV_TYPE_WIDGET, GanvWidgetClass))
#define GANV_IS_WIDGET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GANV_TYPE_WIDGET))
#define GANV_IS_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GANV_TYPE_WIDGET))
#define GANV_WIDGET_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GANV_TYPE_WIDGET, GanvWidgetClass))

struct _GanvWidget;
struct _GanvWidgetClass;

typedef struct _GanvWidget        GanvWidget;
typedef struct _GanvWidgetPrivate GanvWidgetPrivate;
typedef struct _GanvWidgetClass   GanvWidgetClass;

struct _GanvWidget {
	GanvItem           item;
	GanvWidgetPrivate* impl;
};

struct _GanvWidgetClass {
	GanvItemClass parent_class;

	/* Reserved for future expansion */
	gpointer spare_vmethods [4];
};

GType ganv_widget_get_type(void) G_GNUC_CONST;

G_END_DECLS

#endif  /* GANV_WIDGET_H */
