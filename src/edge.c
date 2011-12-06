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

#include <math.h>
#include <string.h>

#include <cairo.h>
#include <libgnomecanvas/libgnomecanvas.h>

#include "ganv/canvas.h"
#include "ganv/edge.h"
#include "ganv/node.h"

#include "./boilerplate.h"
#include "./color.h"
#include "./gettext.h"

#include "ganv-private.h"

// TODO: Very sloppy clipping for arrow heads
#define ARROW_DEPTH   32
#define ARROW_BREADTH 32

// Uncomment to see control point path as straight lines
//#define GANV_DEBUG_CURVES 1

enum {
	PROP_0,
	PROP_TAIL,
	PROP_HEAD,
	PROP_WIDTH,
	PROP_HANDLE_RADIUS,
	PROP_DASH_LENGTH,
	PROP_DASH_OFFSET,
	PROP_COLOR,
	PROP_CURVED,
	PROP_ARROWHEAD,
	PROP_SELECTED,
	PROP_HIGHLIGHTED,
	PROP_GHOST
};

G_DEFINE_TYPE(GanvEdge, ganv_edge, GNOME_TYPE_CANVAS_ITEM)

static GnomeCanvasItemClass* parent_class;

static void
ganv_edge_init(GanvEdge* edge)
{
	edge->tail = NULL;
	edge->head = NULL;

	memset(&edge->coords, '\0', sizeof(GanvEdgeCoords));
	edge->coords.width         = 2.0;
	edge->coords.handle_radius = 4.0;
	edge->coords.curved        = FALSE;
	edge->coords.arrowhead     = FALSE;

	edge->old_coords  = edge->coords;
	edge->dash_length = 0.0;
	edge->dash_offset = 0.0;
	edge->color       = 0xA0A0A0FF;
}

static void
ganv_edge_destroy(GtkObject* object)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(GANV_IS_EDGE(object));

	GanvEdge*   edge   = GANV_EDGE(object);
	GanvCanvas* canvas = GANV_CANVAS(edge->item.canvas);
	if (canvas && !edge->ghost) {
		ganv_canvas_remove_edge(canvas, edge);
		edge->item.canvas = NULL;
	}
	edge->item.parent = NULL;

	if (GTK_OBJECT_CLASS(parent_class)->destroy) {
		(*GTK_OBJECT_CLASS(parent_class)->destroy)(object);
	}
}

static void
ganv_edge_set_property(GObject*      object,
                       guint         prop_id,
                       const GValue* value,
                       GParamSpec*   pspec)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(GANV_IS_EDGE(object));

	GanvEdge*       edge   = GANV_EDGE(object);
	GanvEdgeCoords* coords = &edge->coords;

	switch (prop_id) {
		SET_CASE(TAIL, object, edge->tail);
		SET_CASE(HEAD, object, edge->head);
		SET_CASE(WIDTH, double, coords->width);
		SET_CASE(HANDLE_RADIUS, double, coords->handle_radius);
		SET_CASE(DASH_LENGTH, double, edge->dash_length);
		SET_CASE(DASH_OFFSET, double, edge->dash_offset);
		SET_CASE(COLOR, uint, edge->color);
		SET_CASE(CURVED, boolean, edge->coords.curved);
		SET_CASE(ARROWHEAD, boolean, edge->coords.arrowhead);
		SET_CASE(SELECTED, boolean, edge->selected);
		SET_CASE(HIGHLIGHTED, boolean, edge->highlighted);
		SET_CASE(GHOST, boolean, edge->ghost);
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
ganv_edge_get_property(GObject*    object,
                       guint       prop_id,
                       GValue*     value,
                       GParamSpec* pspec)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(GANV_IS_EDGE(object));

	GanvEdge* edge = GANV_EDGE(object);

	switch (prop_id) {
		GET_CASE(TAIL, object, edge->tail);
		GET_CASE(HEAD, object, edge->head);
		GET_CASE(WIDTH, double, edge->coords.width);
		SET_CASE(HANDLE_RADIUS, double, edge->coords.handle_radius);
		GET_CASE(DASH_LENGTH, double, edge->dash_length);
		GET_CASE(DASH_OFFSET, double, edge->dash_offset);
		GET_CASE(COLOR, uint, edge->color);
		GET_CASE(CURVED, boolean, edge->coords.curved);
		GET_CASE(ARROWHEAD, boolean, edge->coords.arrowhead);
		GET_CASE(SELECTED, boolean, edge->selected);
		GET_CASE(HIGHLIGHTED, boolean, edge->highlighted);
		SET_CASE(GHOST, boolean, edge->ghost);
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
request_redraw(GnomeCanvas*          canvas,
               const GanvEdgeCoords* coords)
{
	const double w  = coords->width;
	if (coords->curved) {
		const double src_x  = coords->x1;
		const double src_y  = coords->y1;
		const double dst_x  = coords->x2;
		const double dst_y  = coords->y2;
		const double join_x = (src_x + dst_x) / 2.0;
		const double join_y = (src_y + dst_y) / 2.0;
		const double src_x1 = coords->cx1;
		const double src_y1 = coords->cy1;
		const double dst_x1 = coords->cx2;
		const double dst_y1 = coords->cy2;

		const double r1x1 = MIN(MIN(src_x, join_x), src_x1);
		const double r1y1 = MIN(MIN(src_y, join_y), src_y1);
		const double r1x2 = MAX(MAX(src_x, join_x), src_x1);
		const double r1y2 = MAX(MAX(src_y, join_y), src_y1);
		gnome_canvas_request_redraw(canvas,
		                            r1x1 - w, r1y1 - w,
		                            r1x2 + w, r1y2 + w);

		const double r2x1 = MIN(MIN(dst_x, join_x), dst_x1);
		const double r2y1 = MIN(MIN(dst_y, join_y), dst_y1);
		const double r2x2 = MAX(MAX(dst_x, join_x), dst_x1);
		const double r2y2 = MAX(MAX(dst_y, join_y), dst_y1);
		gnome_canvas_request_redraw(canvas,
		                            r2x1 - w, r2y1 - w,
		                            r2x2 + w, r2y2 + w);

	} else {
		const double x1 = MIN(coords->x1, coords->x2);
		const double y1 = MIN(coords->y1, coords->y2);
		const double x2 = MAX(coords->x1, coords->x2);
		const double y2 = MAX(coords->y1, coords->y2);

		gnome_canvas_request_redraw(canvas,
		                            x1 - w, y1 - w,
		                            x2 + w, y2 + w);
	}

	gnome_canvas_request_redraw(canvas,
	                            coords->handle_x - coords->handle_radius,
	                            coords->handle_y - coords->handle_radius,
	                            coords->handle_x + coords->handle_radius,
	                            coords->handle_y + coords->handle_radius);

	if (coords->arrowhead) {
		gnome_canvas_request_redraw(
			canvas,
			coords->x2 - ARROW_DEPTH,
			coords->y2 - ARROW_BREADTH,
			coords->x2 + ARROW_DEPTH,
			coords->y2 + ARROW_BREADTH);
	}
}

static void
ganv_edge_bounds(GnomeCanvasItem* item,
                 double* x1, double* y1,
                 double* x2, double* y2)
{
	GanvEdge* edge = GANV_EDGE(item);

	// TODO: This is not correct for curved edges
	*x1 = MIN(edge->coords.x1, edge->coords.x2);
	*y1 = MIN(edge->coords.y1, edge->coords.y2);
	*x2 = MAX(edge->coords.x1, edge->coords.x2);
	*y2 = MAX(edge->coords.y1, edge->coords.y2);
}

static void
ganv_edge_update(GnomeCanvasItem* item,
                 double*          affine,
                 ArtSVP*          clip_path,
                 int              flags)
{
	GanvEdge* edge = GANV_EDGE(item);

	if (parent_class->update) {
		(*parent_class->update)(item, affine, clip_path, flags);
	}

	// Request redraw of old location
	request_redraw(item->canvas, &edge->old_coords);

	// Calculate new coordinates from tail and head
	GanvEdgeCoords* coords = &edge->coords;
	GANV_NODE_GET_CLASS(edge->tail)->tail_vector(
		edge->tail, edge->head,
		&coords->x1, &coords->y1, &coords->cx1, &coords->cy1);
	GANV_NODE_GET_CLASS(edge->head)->head_vector(
		edge->head, edge->tail,
		&coords->x2, &coords->y2, &coords->cx2, &coords->cy2);

	const double dx = coords->x2 - coords->x1;
	const double dy = coords->y2 - coords->y1;
	coords->handle_x = coords->x1 + (dx / 2.0);
	coords->handle_y = coords->y1 + (dy / 2.0);

	coords->cx1  = coords->x1 + (coords->cx1 * (ceilf(fabs(dx)) / 4.0));
	coords->cy1 += coords->y1;
	coords->cx2  = coords->x2 + (coords->cx2 * (ceilf(fabs(dx)) / 4.0));
	coords->cy2 += coords->y2;

	// Update old coordinates
	edge->old_coords = edge->coords;

	// Get bounding box
	double x1, x2, y1, y2;
	ganv_edge_bounds(item, &x1, &y1, &x2, &y2);

	// Ensure bounding box has non-zero area
	if (x1 == x2) {
		x2 += 1.0;
	}
	if (y1 == y2) {
		y2 += 1.0;
	}

	// Update item canvas coordinates
	gnome_canvas_w2c_d(GNOME_CANVAS(item->canvas), x1, y1, &item->x1, &item->y1);
	gnome_canvas_w2c_d(GNOME_CANVAS(item->canvas), x2, y2, &item->x2, &item->y2);

	// Request redraw of new location
	request_redraw(item->canvas, &edge->coords);
}

static void
ganv_edge_render(GnomeCanvasItem* item,
                 GnomeCanvasBuf*  buf)
{
	// Not implemented
}

static void
ganv_edge_draw(GnomeCanvasItem* item,
               GdkDrawable* drawable,
               int x, int y,
               int width, int height)
{
	GanvEdge* me = GANV_EDGE(item);
	cairo_t*  cr = gdk_cairo_create(drawable);

	double src_x  = me->coords.x1 - x;
	double src_y  = me->coords.y1 - y;
	//double src_cx = me->coords.cx1 - x;
	//double src_cy = me->coords.cy1 - y;
	double dst_x  = me->coords.x2 - x;
	double dst_y  = me->coords.y2 - y;
	//double dst_cx = me->coords.cx2 - x;
	//double dst_cy = me->coords.cy2 - y;

	double dx = src_x - dst_x;
	double dy = src_y - dst_y;

	double r, g, b, a;
	if (me->highlighted) {
		color_to_rgba(highlight_color(me->color, 0x20), &r, &g, &b, &a);
	} else {
		color_to_rgba(me->color, &r, &g, &b, &a);
	}
	cairo_set_source_rgba(cr, r, g, b, a);

	cairo_set_line_width(cr, me->coords.width);
	cairo_move_to(cr, src_x, src_y);

	const double dash_length = (me->selected ? 4.0 : me->dash_length);
	if (dash_length > 0.0) {
		double dashed[2] = { dash_length, dash_length };
		cairo_set_dash(cr, dashed, 2, me->dash_offset);
	}

	const double join_x = (src_x + dst_x) / 2.0;
	const double join_y = (src_y + dst_y) / 2.0;

	if (me->coords.curved) {
		// Curved line as 2 paths which join at the middle point

		// Path 1 (src_x, src_y) -> (join_x, join_y)
		// Control point 1
		const double src_x1 = me->coords.cx1 - x;
		const double src_y1 = me->coords.cy1 - y;
		// Control point 2
		const double src_x2 = (join_x + src_x1) / 2.0;
		const double src_y2 = (join_y + src_y1) / 2.0;

		// Path 2, (join_x, join_y) -> (dst_x, dst_y)
		// Control point 1
		const double dst_x1 = me->coords.cx2 - x;
		const double dst_y1 = me->coords.cy2 - y;
		// Control point 2
		const double dst_x2 = (join_x + dst_x1) / 2.0;
		const double dst_y2 = (join_y + dst_y1) / 2.0;

		cairo_move_to(cr, src_x, src_y);
		cairo_curve_to(cr, src_x1, src_y1, src_x2, src_y2, join_x, join_y);
		cairo_curve_to(cr, dst_x2, dst_y2, dst_x1, dst_y1, dst_x, dst_y);

		if (me->coords.arrowhead) {
			cairo_line_to(cr, dst_x - 12, dst_y - 4);
			cairo_move_to(cr, dst_x, dst_y);
			cairo_line_to(cr, dst_x - 12, dst_y + 4);
		}

#ifdef GANV_DEBUG_CURVES
		cairo_stroke(cr);
		cairo_save(cr);
		cairo_set_source_rgba(cr, 1.0, 0, 0, 0.5);

		cairo_move_to(cr, src_x, src_y);
		cairo_line_to(cr, src_x1, src_y1);
		cairo_stroke(cr);

		cairo_move_to(cr, join_x, join_y);
		cairo_line_to(cr, src_x2, src_y2);
		cairo_stroke(cr);

		cairo_move_to(cr, join_x, join_y);
		cairo_line_to(cr, dst_x2, dst_y2);
		cairo_stroke(cr);

		cairo_move_to(cr, dst_x, dst_y);
		cairo_line_to(cr, dst_x1, dst_y1);
		cairo_stroke(cr);
		cairo_restore(cr);
#endif

	} else {
		// Straight line from (x1, y1) to (x2, y2)
		cairo_move_to(cr, src_x, src_y);
		cairo_line_to(cr, dst_x, dst_y);

		if (me->coords.arrowhead) {
			const double ah  = sqrt(dx * dx + dy * dy);
			const double adx = dx / ah * 10.0;
			const double ady = dy / ah * 10.0;

			cairo_line_to(cr,
			              dst_x + adx - ady/1.5,
			              dst_y + ady + adx/1.5);
			cairo_move_to(cr, dst_x, dst_y);
			cairo_line_to(cr,
			              dst_x + adx + ady/1.5,
			              dst_y + ady - adx/1.5);
		}
	}

	cairo_stroke(cr);

	cairo_move_to(cr, join_x, join_y);
	cairo_arc(cr, join_x, join_y, me->coords.handle_radius, 0, 2 * M_PI);
	cairo_fill(cr);

	cairo_destroy(cr);
}

static double
ganv_edge_point(GnomeCanvasItem* item,
                double x, double y,
                int cx, int cy,
                GnomeCanvasItem** actual_item)
{
	const GanvEdge*       edge = GANV_EDGE(item);
	const GanvEdgeCoords* coords = &edge->coords;

	const double dx = fabs(x - coords->handle_x);
	const double dy = fabs(y - coords->handle_y);
	const double d  = sqrt((dx * dx) + (dy * dy));

	*actual_item = item;

	if (d <= coords->handle_radius) {
		// Point is inside the handle
		return 0.0;
	} else {
		// Distance from the edge of the handle
		return d - (coords->handle_radius + coords->width);
	}
}

gboolean
ganv_edge_is_within(const GanvEdge* edge,
                    double          x1,
                    double          y1,
                    double          x2,
                    double          y2)
{
	const double handle_x = edge->coords.handle_x;
	const double handle_y = edge->coords.handle_y;

	return handle_x >= x1
		&& handle_x <= x2
		&& handle_y >= y1
		&& handle_y <= y2;
}

static void
ganv_edge_class_init(GanvEdgeClass* class)
{
	GObjectClass*         gobject_class = (GObjectClass*)class;
	GtkObjectClass*       object_class  = (GtkObjectClass*)class;
	GnomeCanvasItemClass* item_class    = (GnomeCanvasItemClass*)class;

	parent_class = GNOME_CANVAS_ITEM_CLASS(g_type_class_peek_parent(class));

	gobject_class->set_property = ganv_edge_set_property;
	gobject_class->get_property = ganv_edge_get_property;

	g_object_class_install_property(
		gobject_class, PROP_TAIL, g_param_spec_object(
			"tail",
			_("tail"),
			_("node this edge starts from"),
			GANV_TYPE_NODE,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_HEAD, g_param_spec_object(
			"head",
			_("head"),
			_("node this edge ends at"),
			GANV_TYPE_NODE,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_WIDTH, g_param_spec_double(
			"width",
			_("line width"),
			_("width of line in canvas units"),
			0.0, G_MAXDOUBLE,
			2.0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_HANDLE_RADIUS, g_param_spec_double(
			"handle-radius",
			_("handle radius"),
			_("radius of handle in canvas units"),
			0.0, G_MAXDOUBLE,
			4.0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_DASH_LENGTH, g_param_spec_double(
			"dash-length",
			_("line dash length"),
			_("length of dashes, or zero for no dashing"),
			0.0, G_MAXDOUBLE,
			0.0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_DASH_OFFSET, g_param_spec_double(
			"dash-offset",
			_("line dash offset"),
			_("offset for dashes (useful for 'rubber band' animation)."),
			0.0, G_MAXDOUBLE,
			0.0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_COLOR, g_param_spec_uint(
			"color",
			_("color"),
			_("color as an RGBA integer"),
			0, G_MAXUINT,
			0xA0A0A0FF,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_CURVED, g_param_spec_boolean(
			"curved",
			_("curved"),
			_("whether line should be curved, not straight"),
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_ARROWHEAD, g_param_spec_boolean(
			"arrowhead",
			_("arrowhead"),
			_("whether to show an arrowhead at the end point"),
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_SELECTED, g_param_spec_boolean(
			"selected",
			_("selected"),
			_("whether this edge is selected"),
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_HIGHLIGHTED, g_param_spec_boolean(
			"highlighted",
			_("highlighted"),
			_("whether to highlight the edge"),
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_GHOST, g_param_spec_boolean(
			"ghost",
			_("ghost"),
			_("whether to highlight the edge"),
			0,
			G_PARAM_READWRITE));

	object_class->destroy = ganv_edge_destroy;

	item_class->update = ganv_edge_update;
	item_class->bounds = ganv_edge_bounds;
	item_class->point  = ganv_edge_point;
	item_class->render = ganv_edge_render;
	item_class->draw   = ganv_edge_draw;
}

GanvEdge*
ganv_edge_new(GanvCanvas* canvas,
              GanvNode*   tail,
              GanvNode*   head,
              const char* first_prop_name, ...)
{
	GanvEdge* edge = GANV_EDGE(
		g_object_new(ganv_edge_get_type(), NULL));

	va_list args;
	va_start(args, first_prop_name);
	gnome_canvas_item_construct(&edge->item,
	                            ganv_canvas_get_root(canvas),
	                            first_prop_name, args);
	va_end(args);

	edge->tail = tail;
	edge->head = head;

	if (!edge->ghost) {
		ganv_canvas_add_edge(canvas, edge);
	}
	return edge;
}

void
ganv_edge_update_location(GanvEdge* edge)
{
	gnome_canvas_item_request_update(GNOME_CANVAS_ITEM(edge));
}

void
ganv_edge_select(GanvEdge* edge)
{
	GanvCanvas* canvas = GANV_CANVAS(edge->item.canvas);
	ganv_canvas_select_edge(canvas, edge);
}

void
ganv_edge_unselect(GanvEdge* edge)
{
	GanvCanvas* canvas = GANV_CANVAS(edge->item.canvas);
	ganv_canvas_unselect_edge(canvas, edge);
}

void
ganv_edge_highlight(GanvEdge* edge)
{
	edge->highlighted = TRUE;
	gnome_canvas_item_raise_to_top(GNOME_CANVAS_ITEM(edge));
	gnome_canvas_item_request_update(GNOME_CANVAS_ITEM(edge));
}

void
ganv_edge_unhighlight(GanvEdge* edge)
{
	edge->highlighted = FALSE;
	gnome_canvas_item_request_update(GNOME_CANVAS_ITEM(edge));
}

void
ganv_edge_tick(GanvEdge* edge,
               double    seconds)
{
	gnome_canvas_item_set(GNOME_CANVAS_ITEM(edge),
	                      "dash-offset", seconds * 8.0,
	                      NULL);
}

void
ganv_edge_remove(GanvEdge* edge)
{
	if (!edge->ghost) {
		ganv_canvas_remove_edge(
			GANV_CANVAS(edge->item.canvas),
			edge);
	}
}
