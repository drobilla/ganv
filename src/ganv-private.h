/* This file is part of Ganv.
 * Copyright 2007-2013 David Robillard <http://drobilla.net>
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

#ifndef GANV_PRIVATE_H
#define GANV_PRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <cairo.h>

#include "ganv/types.h"
#include "ganv/text.h"

extern guint signal_moved;

/* Box */

typedef struct {
	double   x1, y1, x2, y2;
	double   border_width;
	gboolean stacked;
} GanvBoxCoords;

struct _GanvBoxImpl {
	GanvBoxCoords coords;
	GanvBoxCoords old_coords;
	double        radius_tl;
	double        radius_tr;
	double        radius_br;
	double        radius_bl;
};

/* Circle */

typedef struct {
	double x, y, radius, radius_ems;
	double width;
} GanvCircleCoords;

struct _GanvCircleImpl {
	GanvCircleCoords coords;
	GanvCircleCoords old_coords;
	gboolean         fit_label;
};

/* Edge */

typedef struct {
	double   x1, y1, x2, y2;
	double   cx1, cy1, cx2, cy2;
	double   handle_x, handle_y, handle_radius;
	double   width;
	gboolean curved;
	gboolean arrowhead;
} GanvEdgeCoords;

struct _GanvEdgeImpl
{
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

/* Module */

struct _GanvModuleImpl
{
	GPtrArray* ports;
	GanvItem*  embed_item;
	int        embed_width;
	int        embed_height;
	double     widest_input;
	double     widest_output;
	gboolean   must_resize;
};

/* Node */

struct _GanvNodeImpl {
	struct _GanvNode* partner;
	GanvText*         label;
	double            dash_length;
	double            dash_offset;
	double            border_width;
	guint             fill_color;
	guint             border_color;
	guint             layer;
	gboolean          can_tail;
	gboolean          can_head;
	gboolean          selected;
	gboolean          highlighted;
	gboolean          draggable;
	gboolean          show_label;
	gboolean          grabbed;
#ifdef GANV_FDGL
	Vector            force;
	Vector            vel;
#endif
};

/* Port */

typedef struct {
	GanvBox*  rect;
	GanvText* label;
	float     value;
	float     min;
	float     max;
	gboolean  is_toggle;
	gboolean  is_integer;
} GanvPortControl;

struct _GanvPortImpl {
	GanvPortControl* control;
	gboolean         is_input;
};

/* Text */

typedef struct
{
	double x;
	double y;
	double width;
	double height;
} GanvTextCoords;

struct _GanvTextImpl
{
	cairo_surface_t* surface;
	char*            text;
	GanvTextCoords   coords;
	GanvTextCoords   old_coords;
	guint            color;
	gboolean         needs_layout;
};

/* Canvas */

void
ganv_canvas_move_selected_items(GanvCanvas* canvas,
                                double      dx,
                                double      dy);

void
ganv_canvas_selection_move_finished(GanvCanvas* canvas);

void
ganv_canvas_select_node(GanvCanvas* canvas,
                        GanvNode*   node);

void
ganv_canvas_unselect_node(GanvCanvas* canvas,
                          GanvNode*   node);

void
ganv_canvas_add_edge(GanvCanvas* canvas,
                     GanvEdge*   edge);

void
ganv_canvas_remove_edge(GanvCanvas* canvas,
                        GanvEdge*   edge);

void
ganv_canvas_select_edge(GanvCanvas* canvas,
                        GanvEdge*   edge);

void
ganv_canvas_unselect_edge(GanvCanvas* canvas,
                          GanvEdge*   edge);

gboolean
ganv_canvas_port_event(GanvCanvas* canvas,
                       GanvPort*   port,
                       GdkEvent*   event);

void
ganv_item_invoke_update(GanvItem* item, int flags);

/* Edge */

void
ganv_edge_request_redraw(GanvCanvasBase*       canvas,
                         const GanvEdgeCoords* coords);

/* Box */

void
ganv_box_request_redraw(GanvItem*            item,
                        const GanvBoxCoords* coords,
                        gboolean             world);

/* Port */

void
ganv_port_set_control_value_internal(GanvPort* port,
                                     float     value);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* GANV_PRIVATE_H */
