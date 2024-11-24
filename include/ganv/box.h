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

#ifndef GANV_BOX_H
#define GANV_BOX_H

#include <ganv/node.h>
#include <ganv/types.h>

#include <glib-object.h>
#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GANV_TYPE_BOX            (ganv_box_get_type())
#define GANV_BOX(obj)            (GTK_CHECK_CAST((obj), GANV_TYPE_BOX, GanvBox))
#define GANV_BOX_CLASS(klass)    (GTK_CHECK_CLASS_CAST((klass), GANV_TYPE_BOX, GanvBoxClass))
#define GANV_IS_BOX(obj)         (GTK_CHECK_TYPE((obj), GANV_TYPE_BOX))
#define GANV_IS_BOX_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GANV_TYPE_BOX))
#define GANV_BOX_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS((obj), GANV_TYPE_BOX, GanvBoxClass))

struct _GanvBoxClass;

typedef struct _GanvBoxClass   GanvBoxClass;
typedef struct _GanvBoxPrivate GanvBoxPrivate;

struct _GanvBox {
	GanvNode        node;
	GanvBoxPrivate* impl;
};

/**
 * GanvBoxClass:
 * @parent_class: Node superclass.
 * @set_width: Set the width of the box.
 * @set_height: Set the height of the box.
 */
struct _GanvBoxClass {
	GanvNodeClass parent_class;

	void (*set_width)(GanvBox* box,
	                  double   width);

	void (*set_height)(GanvBox* box,
	                   double   height);

	/* Reserved for future expansion */
	gpointer spare_vmethods[4];
};

GType  ganv_box_get_type(void) G_GNUC_CONST;
double ganv_box_get_x1(const GanvBox* box);
double ganv_box_get_y1(const GanvBox* box);
double ganv_box_get_x2(const GanvBox* box);
double ganv_box_get_y2(const GanvBox* box);
double ganv_box_get_width(const GanvBox* box);
void   ganv_box_set_width(GanvBox* box, double width);
double ganv_box_get_height(const GanvBox* box);
void   ganv_box_set_height(GanvBox* box, double height);
double ganv_box_get_border_width(const GanvBox* box);

/**
 * ganv_box_normalize:
 * @box: The box to normalize.
 *
 * Normalize the box coordinates such that x1 < x2 and y1 < y2.
 */
void ganv_box_normalize(GanvBox* box);

G_END_DECLS

#endif  /* GANV_BOX_H */
