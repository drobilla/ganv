/* This file is part of Ganv.
 * Copyright 2007-2012 David Robillard <http://drobilla.net>
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

/* Based on GnomeCanvasWidget, by Federico Mena <federico@nuclecu.unam.mx>
 * Copyright 1997-2000 Free Software Foundation
 */

#ifndef GANV_WIDGET_H
#define GANV_WIDGET_H

#include "ganv/item.h"

G_BEGIN_DECLS

#define GANV_TYPE_WIDGET            (ganv_widget_get_type ())
#define GANV_WIDGET(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GANV_TYPE_WIDGET, GanvWidget))
#define GANV_WIDGET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GANV_TYPE_WIDGET, GanvWidgetClass))
#define GANV_IS_WIDGET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GANV_TYPE_WIDGET))
#define GANV_IS_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GANV_TYPE_WIDGET))
#define GANV_WIDGET_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GANV_TYPE_WIDGET, GanvWidgetClass))

typedef struct _GanvWidget GanvWidget;
typedef struct _GanvWidgetClass GanvWidgetClass;

struct _GanvWidget {
	GanvItem item;

	GtkWidget* widget;          /* The child widget */

	double        x, y;			/* Position at anchor */
	double        width, height; /* Dimensions of widget */
	GtkAnchorType anchor;		/* Anchor side for widget */

	int cx, cy;                 /* Top-left canvas coordinates for widget */
	int cwidth, cheight;		/* Size of widget in pixels */

	guint destroy_id;           /* Signal connection id for destruction of child widget */

	guint size_pixels : 1;		/* Is size specified in (unchanging) pixels or units (get scaled)? */
	guint in_destroy : 1;		/* Is child widget being destroyed? */
};

struct _GanvWidgetClass {
	GanvItemClass parent_class;
};

GType ganv_widget_get_type(void);

G_END_DECLS

#endif  /* GANV_WIDGET_H */
