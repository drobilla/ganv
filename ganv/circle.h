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

#ifndef GANV_CIRCLE_H
#define GANV_CIRCLE_H

#include "ganv/node.h"

G_BEGIN_DECLS

#define GANV_TYPE_CIRCLE             (ganv_circle_get_type())
#define GANV_CIRCLE(obj)             (GTK_CHECK_CAST((obj), GANV_TYPE_CIRCLE, GanvCircle))
#define GANV_CIRCLE_CLASS(klass)     (GTK_CHECK_CLASS_CAST((klass), GANV_TYPE_CIRCLE, GanvCircleClass))
#define GANV_IS_CIRCLE(obj)          (GTK_CHECK_TYPE((obj), GANV_TYPE_CIRCLE))
#define GANV_IS_CIRCLE_CLASS(klass)  (GTK_CHECK_CLASS_TYPE((klass), GANV_TYPE_CIRCLE))
#define GANV_CIRCLE_GET_CLASS(obj)   (GTK_CHECK_GET_CLASS((obj), GANV_TYPE_CIRCLE, GanvCircleClass))
#define GANV_CIRCLE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), GANV_TYPE_CIRCLE, GanvCircleImpl))

typedef struct _GanvCircle      GanvCircle;
typedef struct _GanvCircleClass GanvCircleClass;
typedef struct _GanvCircleImpl  GanvCircleImpl;

struct _GanvCircle
{
	GanvNode        node;
	GanvCircleImpl* impl;
};

struct _GanvCircleClass {
	GanvNodeClass parent_class;
};

GType ganv_circle_get_type(void);

G_END_DECLS

#endif  /* GANV_CIRCLE_H */
