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

#ifndef GANV_EDGE_H
#define GANV_EDGE_H

#include "ganv/item.h"
#include "ganv/node.h"
#include "ganv/types.h"

G_BEGIN_DECLS

#define GANV_TYPE_EDGE            (ganv_edge_get_type())
#define GANV_EDGE(obj)            (GTK_CHECK_CAST((obj), GANV_TYPE_EDGE, GanvEdge))
#define GANV_EDGE_CLASS(klass)    (GTK_CHECK_CLASS_CAST((klass), GANV_TYPE_EDGE, GanvEdgeClass))
#define GANV_IS_EDGE(obj)         (GTK_CHECK_TYPE((obj), GANV_TYPE_EDGE))
#define GANV_IS_EDGE_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GANV_TYPE_EDGE))
#define GANV_EDGE_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS((obj), GANV_TYPE_EDGE, GanvEdgeClass))

typedef struct _GanvEdgeClass GanvEdgeClass;
typedef struct _GanvEdgeImpl  GanvEdgeImpl;

struct _GanvEdge {
	GanvItem      item;
	GanvEdgeImpl* impl;
};

struct _GanvEdgeClass {
	GanvItemClass parent_class;

	/* Reserved for future expansion */
	gpointer spare_vmethods[4];
};

GType ganv_edge_get_type(void) G_GNUC_CONST;

GanvEdge*
ganv_edge_new(GanvCanvas* canvas,
              GanvNode*   tail,
              GanvNode*   head,
              const char* first_prop_name, ...);

gboolean
ganv_edge_is_within(const GanvEdge* edge,
                    double          x1,
                    double          y1,
                    double          x2,
                    double          y2);

gboolean
ganv_edge_get_curved(const GanvEdge* edge);

void
ganv_edge_set_curved(GanvEdge* edge, gboolean curved);

void
ganv_edge_set_selected(GanvEdge* edge, gboolean selected);

void
ganv_edge_set_highlighted(GanvEdge* edge, gboolean highlighted);

void
ganv_edge_select(GanvEdge* edge);

void
ganv_edge_unselect(GanvEdge* edge);

/**
 * ganv_edge_disconnect:
 *
 * Disconnect the edge.  This will disconnect the edge just as if it had been
 * disconnected by the user via the canvas.  The canvas disconnect signal will
 * be emitted, allowing the application to control disconnect logic.
 */
void
ganv_edge_disconnect(GanvEdge* edge);

/**
 * ganv_edge_remove:
 *
 * Remove the edge from the canvas.  This will only remove the edge visually,
 * it will not emit the canvas disconnect signal to notify the application.
 */
void
ganv_edge_remove(GanvEdge* edge);

/**
 * ganv_edge_get_tail:
 *
 * Return value: (transfer none): The tail of @a edge.
 */
GanvNode*
ganv_edge_get_tail(const GanvEdge* edge);

/**
 * ganv_edge_get_head:
 *
 * Return value: (transfer none): The head of @a edge.
 */
GanvNode*
ganv_edge_get_head(const GanvEdge* edge);

G_END_DECLS

#endif  /* GANV_EDGE_H */
