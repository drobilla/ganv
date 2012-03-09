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

#ifndef GANV_EDGE_H
#define GANV_EDGE_H

#include "ganv/canvas-base.h"
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
typedef struct _GanvEdgeImpl GanvEdgeImpl;

struct _GanvEdge {
	GanvItem      item;
	GanvEdgeImpl* impl;
};

struct _GanvEdgeClass {
	GanvItemClass parent_class;
};

GType ganv_edge_get_type(void);

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

void
ganv_edge_update_location(GanvEdge* edge);

void
ganv_edge_select(GanvEdge* edge);

void
ganv_edge_unselect(GanvEdge* edge);

void
ganv_edge_highlight(GanvEdge* edge);

void
ganv_edge_unhighlight(GanvEdge* edge);

/**
 * ganv_edge_disconnect:
 * Disconnect the edge.  This will disconnect the edge just as if it had been
 * disconnected by the user via the canvas.  The canvas disconnect signal will
 * be emitted, allowing the application to control disconnect logic.
 */
void
ganv_edge_disconnect(GanvEdge* edge);

/**
 * ganv_edge_remove:
 * Remove the edge from the canvas.  This will only remove the edge visually,
 * it will not emit the canvas disconnect signal to notify the application.
 */
void
ganv_edge_remove(GanvEdge* edge);

void
ganv_edge_tick(GanvEdge* edge,
               double    seconds);

/**
 * ganv_edge_get_tail:
 * Return value: (transfer none): The tail of @a edge.
 */
GanvNode*
ganv_edge_get_tail(const GanvEdge* edge);

/**
 * ganv_edge_get_head:
 * Return value: (transfer none): The head of @a edge.
 */
GanvNode*
ganv_edge_get_head(const GanvEdge* edge);

G_END_DECLS

#endif  /* GANV_EDGE_H */
