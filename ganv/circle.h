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

#ifndef GANV_CIRCLE_H
#define GANV_CIRCLE_H

#include "ganv/node.h"

G_BEGIN_DECLS

#define GANV_TYPE_CIRCLE            (ganv_circle_get_type())
#define GANV_CIRCLE(obj)            (GTK_CHECK_CAST((obj), GANV_TYPE_CIRCLE, GanvCircle))
#define GANV_CIRCLE_CLASS(klass)    (GTK_CHECK_CLASS_CAST((klass), GANV_TYPE_CIRCLE, GanvCircleClass))
#define GANV_IS_CIRCLE(obj)         (GTK_CHECK_TYPE((obj), GANV_TYPE_CIRCLE))
#define GANV_IS_CIRCLE_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GANV_TYPE_CIRCLE))
#define GANV_CIRCLE_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS((obj), GANV_TYPE_CIRCLE, GanvCircleClass))

typedef struct _GanvCircle      GanvCircle;
typedef struct _GanvCircleClass GanvCircleClass;
typedef struct _GanvCircleImpl  GanvCircleImpl;

/**
 * GanvCircle:
 *
 * A circular #GanvNode.  A #GanvCircle is a leaf, that is, it does not contain
 * any child nodes (though, like any #GanvNode, it may have a label).
 */
struct _GanvCircle {
	GanvNode        node;
	GanvCircleImpl* impl;
};

struct _GanvCircleClass {
	GanvNodeClass parent_class;

	/* Reserved for future expansion */
	gpointer spare_vmethods[4];
};

GType ganv_circle_get_type(void) G_GNUC_CONST;

GanvCircle*
ganv_circle_new(GanvCanvas* canvas,
                const char* first_prop_name, ...);

double
ganv_circle_get_radius(const GanvCircle* circle);

void
ganv_circle_set_radius(GanvCircle* circle, double radius);

double
ganv_circle_get_radius_ems(const GanvCircle* circle);

void
ganv_circle_set_radius_ems(GanvCircle* circle, double radius);

gboolean
ganv_circle_get_fit_label(const GanvCircle* circle);

void
ganv_circle_set_fit_label(GanvCircle* circle, gboolean fit_label);

G_END_DECLS

#endif  /* GANV_CIRCLE_H */
