/* This file is part of Ganv.
 * Copyright 2007-2011 David Robillard <http://drobilla.net>
 *
 * Ganv is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Ganv is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Ganv.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GANV_EDGE_H
#define GANV_EDGE_H

#include <libgnomecanvas/libgnomecanvas.h>

#include "ganv/types.h"
#include "ganv/node.h"

G_BEGIN_DECLS

#define GANV_TYPE_EDGE            (ganv_edge_get_type())
#define GANV_EDGE(obj)            (GTK_CHECK_CAST((obj), GANV_TYPE_EDGE, GanvEdge))
#define GANV_EDGE_CLASS(klass)    (GTK_CHECK_CLASS_CAST((klass), GANV_TYPE_EDGE, GanvEdgeClass))
#define GANV_IS_EDGE(obj)         (GTK_CHECK_TYPE((obj), GANV_TYPE_EDGE))
#define GANV_IS_EDGE_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GANV_TYPE_EDGE))
#define GANV_EDGE_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS((obj), GANV_TYPE_EDGE, GanvEdgeClass))

typedef struct _GanvEdgeClass GanvEdgeClass;

typedef struct {
	double   x1, y1, x2, y2;
	double   cx1, cy1, cx2, cy2;
	double   handle_x, handle_y, handle_radius;
	double   width;
	gboolean curved;
	gboolean arrowhead;
} GanvEdgeCoords;

struct _GanvEdge
{
	GnomeCanvasItem item;
	GanvNode*       tail;
	GanvNode*       head;
	GanvEdgeCoords  coords;
	GanvEdgeCoords  old_coords;
	double          dash_length;
	double          dash_offset;
	guint           color;
	gboolean        selected;
	gboolean        highlighted;
	gboolean        ghost;
};

struct _GanvEdgeClass {
	GnomeCanvasItemClass parent_class;
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

void
ganv_edge_remove(GanvEdge* edge);

void
ganv_edge_tick(GanvEdge* edge,
               double    seconds);

static inline GanvNode*
ganv_edge_get_tail(const GanvEdge* edge)
{
	return edge->tail;
}

static inline GanvNode*
ganv_edge_get_head(const GanvEdge* edge)
{
	return edge->head;
}
	
G_END_DECLS

#endif  /* GANV_EDGE_H */
