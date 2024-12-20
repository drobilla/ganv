/* This file is part of Ganv.
 * Copyright 2007-2016 David Robillard <http://drobilla.net>
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

#include "boilerplate.h"
#include "color.h"
#include "ganv-private.h"
#include "gettext.h"

#include <ganv/canvas.h>
#include <ganv/edge.h>
#include <ganv/item.h>
#include <ganv/node.h>
#include <ganv/types.h>

#include <cairo.h>
#include <glib-object.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <stdarg.h>

#include <math.h>
#include <string.h>

#define ARROW_DEPTH   32
#define ARROW_BREADTH 32

// Uncomment to see control point path as straight lines
//#define GANV_DEBUG_CURVES 1

// Uncomment along with GANV_DEBUG_CURVES to see bounding box (buggy)
//#define GANV_DEBUG_BOUNDS 1

enum {
	PROP_0,
	PROP_TAIL,
	PROP_HEAD,
	PROP_WIDTH,
	PROP_HANDLE_RADIUS,
	PROP_DASH_LENGTH,
	PROP_DASH_OFFSET,
	PROP_COLOR,
	PROP_CONSTRAINING,
	PROP_CURVED,
	PROP_ARROWHEAD,
	PROP_SELECTED,
	PROP_HIGHLIGHTED,
	PROP_GHOST
};

G_DEFINE_TYPE_WITH_CODE(GanvEdge, ganv_edge, GANV_TYPE_ITEM,
                        G_ADD_PRIVATE(GanvEdge))

static GanvItemClass* parent_class;

static void
ganv_edge_init(GanvEdge* edge)
{
	GanvEdgePrivate* impl =
	    (GanvEdgePrivate*)ganv_edge_get_instance_private(edge);

	edge->impl = impl;

	impl->tail = NULL;
	impl->head = NULL;

	memset(&impl->coords, '\0', sizeof(GanvEdgeCoords));
	impl->coords.width         = 2.0;
	impl->coords.handle_radius = 4.0;
	impl->coords.constraining  = TRUE;
	impl->coords.curved        = FALSE;
	impl->coords.arrowhead     = FALSE;

	impl->old_coords  = impl->coords;
	impl->dash_length = 0.0;
	impl->dash_offset = 0.0;
	impl->color       = 0;
}

static void
ganv_edge_destroy(GtkObject* object)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(GANV_IS_EDGE(object));

	GanvEdge*   edge   = GANV_EDGE(object);
	GanvCanvas* canvas = GANV_CANVAS(edge->item.impl->canvas);
	if (canvas && !edge->impl->ghost) {
		edge->item.impl->canvas = NULL;
	}
	edge->item.impl->parent = NULL;

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

	GanvItem*        item   = GANV_ITEM(object);
	GanvEdge*        edge   = GANV_EDGE(object);
	GanvEdgePrivate* impl   = edge->impl;
	GanvEdgeCoords*  coords = &impl->coords;

	switch (prop_id) {
		SET_CASE(WIDTH, double, coords->width)
		SET_CASE(HANDLE_RADIUS, double, coords->handle_radius)
		SET_CASE(DASH_LENGTH, double, impl->dash_length)
		SET_CASE(DASH_OFFSET, double, impl->dash_offset)
		SET_CASE(COLOR, uint, impl->color)
		SET_CASE(CONSTRAINING, boolean, impl->coords.constraining)
		SET_CASE(CURVED, boolean, impl->coords.curved)
		SET_CASE(ARROWHEAD, boolean, impl->coords.arrowhead)
		SET_CASE(SELECTED, boolean, impl->selected)
		SET_CASE(HIGHLIGHTED, boolean, impl->highlighted)
		SET_CASE(GHOST, boolean, impl->ghost)
	case PROP_TAIL: {
		const gobject tmp = g_value_get_object(value);
		if (impl->tail != tmp) {
			impl->tail = GANV_NODE(tmp);
			ganv_item_request_update(item);
		}
		break;
	}
	case PROP_HEAD: {
		const gobject tmp = g_value_get_object(value);
		if (impl->head != tmp) {
			impl->head = GANV_NODE(tmp);
			ganv_item_request_update(item);
		}
		break;
	}
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

	GanvEdge*        edge = GANV_EDGE(object);
	GanvEdgePrivate* impl = edge->impl;

	switch (prop_id) {
		GET_CASE(TAIL, object, impl->tail)
		GET_CASE(HEAD, object, impl->head)
		GET_CASE(WIDTH, double, impl->coords.width)
		SET_CASE(HANDLE_RADIUS, double, impl->coords.handle_radius)
		GET_CASE(DASH_LENGTH, double, impl->dash_length)
		GET_CASE(DASH_OFFSET, double, impl->dash_offset)
		GET_CASE(COLOR, uint, impl->color)
		GET_CASE(CONSTRAINING, boolean, impl->coords.constraining)
		GET_CASE(CURVED, boolean, impl->coords.curved)
		GET_CASE(ARROWHEAD, boolean, impl->coords.arrowhead)
		GET_CASE(SELECTED, boolean, impl->selected)
		GET_CASE(HIGHLIGHTED, boolean, impl->highlighted)
		SET_CASE(GHOST, boolean, impl->ghost)
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

void
ganv_edge_request_redraw(GanvItem*             item,
                         const GanvEdgeCoords* coords)
{
	GanvCanvas* canvas = item->impl->canvas;
	const double w = coords->width;
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
		ganv_canvas_request_redraw_w(canvas,
		                             r1x1 - w, r1y1 - w,
		                             r1x2 + w, r1y2 + w);

		const double r2x1 = MIN(MIN(dst_x, join_x), dst_x1);
		const double r2y1 = MIN(MIN(dst_y, join_y), dst_y1);
		const double r2x2 = MAX(MAX(dst_x, join_x), dst_x1);
		const double r2y2 = MAX(MAX(dst_y, join_y), dst_y1);
		ganv_canvas_request_redraw_w(canvas,
		                             r2x1 - w, r2y1 - w,
		                             r2x2 + w, r2y2 + w);

	} else {
		const double x1 = MIN(coords->x1, coords->x2);
		const double y1 = MIN(coords->y1, coords->y2);
		const double x2 = MAX(coords->x1, coords->x2);
		const double y2 = MAX(coords->y1, coords->y2);

		ganv_canvas_request_redraw_w(canvas,
		                             x1 - w, y1 - w,
		                             x2 + w, y2 + w);
	}

	if (coords->handle_radius > 0.0) {
		ganv_canvas_request_redraw_w(
			canvas,
			coords->handle_x - coords->handle_radius - w,
			coords->handle_y - coords->handle_radius - w,
			coords->handle_x + coords->handle_radius + w,
			coords->handle_y + coords->handle_radius + w);
	}

	if (coords->arrowhead) {
		ganv_canvas_request_redraw_w(
			canvas,
			coords->x2 - ARROW_DEPTH,
			coords->y2 - ARROW_BREADTH,
			coords->x2 + ARROW_DEPTH,
			coords->y2 + ARROW_BREADTH);
	}
}

static void
ganv_edge_bounds(GanvItem* item,
                 double* x1, double* y1,
                 double* x2, double* y2)
{
	GanvEdge*        edge   = GANV_EDGE(item);
	GanvEdgePrivate* impl   = edge->impl;
	GanvEdgeCoords*  coords = &impl->coords;
	const double     w      = coords->width;

	if (coords->curved) {
		*x1 = MIN(coords->x1, MIN(coords->cx1, MIN(coords->x2, coords->cx2))) - w;
		*y1 = MIN(coords->y1, MIN(coords->cy1, MIN(coords->y2, coords->cy2))) - w;
		*x2 = MAX(coords->x1, MAX(coords->cx1, MAX(coords->x2, coords->cx2))) + w;
		*y2 = MAX(coords->y1, MAX(coords->cy1, MAX(coords->y2, coords->cy2))) + w;
	} else {
		*x1 = MIN(impl->coords.x1, impl->coords.x2) - w;
		*y1 = MIN(impl->coords.y1, impl->coords.y2) - w;
		*x2 = MAX(impl->coords.x1, impl->coords.x2) + w;
		*y2 = MAX(impl->coords.y1, impl->coords.y2) + w;
	}
}

void
ganv_edge_get_coords(const GanvEdge* edge, GanvEdgeCoords* coords)
{
	GanvEdgePrivate* impl = edge->impl;

	GANV_NODE_GET_CLASS(impl->tail)->tail_vector(
		impl->tail, impl->head,
		&coords->x1, &coords->y1, &coords->cx1, &coords->cy1);
	GANV_NODE_GET_CLASS(impl->head)->head_vector(
		impl->head, impl->tail,
		&coords->x2, &coords->y2, &coords->cx2, &coords->cy2);

	const double dx = coords->x2 - coords->x1;
	const double dy = coords->y2 - coords->y1;

	coords->handle_x = coords->x1 + (dx / 2.0);
	coords->handle_y = coords->y1 + (dy / 2.0);

	const double abs_dx = fabs(dx);
	const double abs_dy = fabs(dy);

	coords->cx1 = coords->x1 + (coords->cx1 * (abs_dx / 4.0));
	coords->cy1 = coords->y1 + (coords->cy1 * (abs_dy / 4.0));
	coords->cx2 = coords->x2 + (coords->cx2 * (abs_dx / 4.0));
	coords->cy2 = coords->y2 + (coords->cy2 * (abs_dy / 4.0));
}

static void
ganv_edge_update(GanvItem* item, int flags)
{
	GanvEdge*        edge = GANV_EDGE(item);
	GanvEdgePrivate* impl = edge->impl;

	// Request redraw of old location
	ganv_edge_request_redraw(item, &impl->old_coords);

	// Calculate new coordinates from tail and head
	ganv_edge_get_coords(edge, &impl->coords);

	// Update old coordinates
	impl->old_coords = impl->coords;

	// Get bounding box
	double x1 = 0.0;
	double x2 = 0.0;
	double y1 = 0.0;
	double y2 = 0.0;
	ganv_edge_bounds(item, &x1, &y1, &x2, &y2);

	// Ensure bounding box has non-zero area
	if (x1 == x2) {
		x2 += 1.0;
	}
	if (y1 == y2) {
		y2 += 1.0;
	}

	// Update world-relative bounding box
	item->impl->x1 = x1;
	item->impl->y1 = y1;
	item->impl->x2 = x2;
	item->impl->y2 = y2;
	ganv_item_i2w_pair(item, &item->impl->x1, &item->impl->y1, &item->impl->x2, &item->impl->y2);

	// Request redraw of new location
	ganv_edge_request_redraw(item, &impl->coords);

	parent_class->update(item, flags);
}

static void
ganv_edge_draw(GanvItem* item,
               cairo_t* cr, double cx, double cy, double cw, double ch)
{
	(void)cx;
	(void)cy;
	(void)cw;
	(void)ch;

	GanvEdge*        edge = GANV_EDGE(item);
	GanvEdgePrivate* impl = edge->impl;

	double src_x = impl->coords.x1;
	double src_y = impl->coords.y1;
	double dst_x = impl->coords.x2;
	double dst_y = impl->coords.y2;
	double dx    = src_x - dst_x;
	double dy    = src_y - dst_y;

	double r = 0.0;
	double g = 0.0;
	double b = 0.0;
	double a = 0.0;
	if (impl->highlighted) {
		color_to_rgba(highlight_color(impl->color, 0x40), &r, &g, &b, &a);
	} else {
		color_to_rgba(impl->color, &r, &g, &b, &a);
	}
	cairo_set_source_rgba(cr, r, g, b, a);

	cairo_set_line_width(cr, impl->coords.width);
	cairo_move_to(cr, src_x, src_y);

	const double dash_length = (impl->selected ? 4.0 : impl->dash_length);
	if (dash_length > 0.0) {
		double dashed[2] = { dash_length, dash_length };
		cairo_set_dash(cr, dashed, 2, impl->dash_offset);
	} else {
		cairo_set_dash(cr, &dash_length, 0, 0);
	}

	const double join_x = (src_x + dst_x) / 2.0;
	const double join_y = (src_y + dst_y) / 2.0;

	if (impl->coords.curved) {
		// Curved line as 2 paths which join at the middle point

		// Path 1 (src_x, src_y) -> (join_x, join_y)
		// Control point 1
		const double src_x1 = impl->coords.cx1;
		const double src_y1 = impl->coords.cy1;
		// Control point 2
		const double src_x2 = (join_x + src_x1) / 2.0;
		const double src_y2 = (join_y + src_y1) / 2.0;

		// Path 2, (join_x, join_y) -> (dst_x, dst_y)
		// Control point 1
		const double dst_x1 = impl->coords.cx2;
		const double dst_y1 = impl->coords.cy2;
		// Control point 2
		const double dst_x2 = (join_x + dst_x1) / 2.0;
		const double dst_y2 = (join_y + dst_y1) / 2.0;

		cairo_move_to(cr, src_x, src_y);
		cairo_curve_to(cr, src_x1, src_y1, src_x2, src_y2, join_x, join_y);
		cairo_curve_to(cr, dst_x2, dst_y2, dst_x1, dst_y1, dst_x, dst_y);

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

#ifdef GANV_DEBUG_BOUNDS
		double bounds_x1, bounds_y1, bounds_x2, bounds_y2;
		ganv_edge_bounds(item, &bounds_x1, &bounds_y1, &bounds_x2, &bounds_y2);
		cairo_rectangle(cr,
		                bounds_x1, bounds_y1,
		                bounds_x2 - bounds_x1, bounds_y2 - bounds_y1);
#endif

		cairo_restore(cr);
#endif

		cairo_stroke(cr);
		if (impl->coords.arrowhead) {
			cairo_move_to(cr, dst_x - 12, dst_y - 4);
			cairo_line_to(cr, dst_x, dst_y);
			cairo_line_to(cr, dst_x - 12, dst_y + 4);
			cairo_close_path(cr);
			cairo_stroke_preserve(cr);
			cairo_fill(cr);
		}

	} else {
		// Straight line from (x1, y1) to (x2, y2)
		cairo_move_to(cr, src_x, src_y);
		cairo_line_to(cr, dst_x, dst_y);
		cairo_stroke(cr);

		if (impl->coords.arrowhead) {
			const double ah  = sqrt(dx * dx + dy * dy);
			const double adx = dx / ah * 8.0;
			const double ady = dy / ah * 8.0;

			cairo_move_to(cr,
			              dst_x + adx - ady/1.5,
			              dst_y + ady + adx/1.5);
			cairo_set_line_join(cr, CAIRO_LINE_JOIN_BEVEL);
			cairo_line_to(cr, dst_x, dst_y);
			cairo_set_line_join(cr, CAIRO_LINE_JOIN_MITER);
			cairo_line_to(cr,
			              dst_x + adx + ady/1.5,
			              dst_y + ady - adx/1.5);
			cairo_close_path(cr);
			cairo_stroke_preserve(cr);
			cairo_fill(cr);
		}
	}

	if (!ganv_canvas_exporting(item->impl->canvas) &&
	    impl->coords.handle_radius > 0.0) {
		cairo_move_to(cr, join_x, join_y);
		cairo_arc(cr, join_x, join_y, impl->coords.handle_radius, 0, 2 * G_PI);
		cairo_fill(cr);
	}
}

static double
ganv_edge_point(GanvItem* item, double x, double y, GanvItem** actual_item)
{
	const GanvEdge*       edge = GANV_EDGE(item);
	const GanvEdgeCoords* coords = &edge->impl->coords;

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
	const double handle_x = edge->impl->coords.handle_x;
	const double handle_y = edge->impl->coords.handle_y;

	return handle_x >= x1
		&& handle_x <= x2
		&& handle_y >= y1
		&& handle_y <= y2;
}

static void
ganv_edge_class_init(GanvEdgeClass* klass)
{
	GObjectClass*   gobject_class = (GObjectClass*)klass;
	GtkObjectClass* object_class  = (GtkObjectClass*)klass;
	GanvItemClass*  item_class    = (GanvItemClass*)klass;

	parent_class = GANV_ITEM_CLASS(g_type_class_peek_parent(klass));

	gobject_class->set_property = ganv_edge_set_property;
	gobject_class->get_property = ganv_edge_get_property;

	g_object_class_install_property(
		gobject_class, PROP_TAIL, g_param_spec_object(
			"tail",
			_("Tail"),
			_("Node this edge starts from."),
			GANV_TYPE_NODE,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_HEAD, g_param_spec_object(
			"head",
			_("Head"),
			_("Node this edge ends at."),
			GANV_TYPE_NODE,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_WIDTH, g_param_spec_double(
			"width",
			_("Line width"),
			_("Width of edge line."),
			0.0, G_MAXDOUBLE,
			2.0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_HANDLE_RADIUS, g_param_spec_double(
			"handle-radius",
			_("Gandle radius"),
			_("Radius of handle in canvas units."),
			0.0, G_MAXDOUBLE,
			4.0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_DASH_LENGTH, g_param_spec_double(
			"dash-length",
			_("Line dash length"),
			_("Length of line dashes, or zero for no dashing."),
			0.0, G_MAXDOUBLE,
			0.0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_DASH_OFFSET, g_param_spec_double(
			"dash-offset",
			_("Line dash offset"),
			_("Start offset for line dashes, used for selected animation."),
			0.0, G_MAXDOUBLE,
			0.0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_COLOR, g_param_spec_uint(
			"color",
			_("Color"),
			_("Line color as an RGBA integer."),
			0, G_MAXUINT,
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_CONSTRAINING, g_param_spec_boolean(
			"constraining",
			_("Constraining"),
			_("Whether edge should constrain the layout."),
			1,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_CURVED, g_param_spec_boolean(
			"curved",
			_("Curved"),
			_("Whether line should be curved rather than straight."),
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_ARROWHEAD, g_param_spec_boolean(
			"arrowhead",
			_("Arrowhead"),
			_("Whether to show an arrowhead at the head of this edge."),
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_SELECTED, g_param_spec_boolean(
			"selected",
			_("Selected"),
			_("Whether this edge is selected."),
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_HIGHLIGHTED, g_param_spec_boolean(
			"highlighted",
			_("Highlighted"),
			_("Whether to highlight the edge."),
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_GHOST, g_param_spec_boolean(
			"ghost",
			_("Ghost"),
			_("Whether this edge is a `ghost', which is an edge that is not "
			  "added to the canvas data structures.  Ghost edges are used for "
			  "temporary edges that are not considered `real', e.g. the edge "
			  "made while dragging to make a connection."),
			0,
			G_PARAM_READWRITE));

	object_class->destroy = ganv_edge_destroy;

	item_class->update = ganv_edge_update;
	item_class->bounds = ganv_edge_bounds;
	item_class->point  = ganv_edge_point;
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
	ganv_item_construct(&edge->item,
	                    GANV_ITEM(ganv_canvas_root(canvas)),
	                    first_prop_name, args);
	va_end(args);

	edge->impl->tail = tail;
	edge->impl->head = head;

	if (!edge->impl->color) {
		const guint tail_color = GANV_NODE(tail)->impl->fill_color;
		g_object_set(G_OBJECT(edge),
		             "color", EDGE_COLOR(tail_color),
		             NULL);
	}

	if (!edge->impl->ghost) {
		ganv_canvas_add_edge(canvas, edge);
	}
	return edge;
}

void
ganv_edge_update_location(GanvEdge* edge)
{
	ganv_item_request_update(GANV_ITEM(edge));
}

gboolean
ganv_edge_get_constraining(const GanvEdge* edge)
{
	return edge->impl->coords.constraining;
}

void
ganv_edge_set_constraining(GanvEdge* edge, gboolean constraining)
{
	edge->impl->coords.constraining = constraining;
	ganv_edge_request_redraw(GANV_ITEM(edge), &edge->impl->coords);
}

gboolean
ganv_edge_get_curved(const GanvEdge* edge)
{
	return edge->impl->coords.curved;
}

void
ganv_edge_set_curved(GanvEdge* edge, gboolean curved)
{
	edge->impl->coords.curved = curved;
	ganv_edge_request_redraw(GANV_ITEM(edge), &edge->impl->coords);
}

void
ganv_edge_set_selected(GanvEdge* edge, gboolean selected)
{
	GanvCanvas* canvas = GANV_CANVAS(edge->item.impl->canvas);
	if (selected) {
		ganv_canvas_select_edge(canvas, edge);
	} else {
		ganv_canvas_unselect_edge(canvas, edge);
	}
}

void
ganv_edge_select(GanvEdge* edge)
{
	ganv_edge_set_selected(edge, TRUE);
}

void
ganv_edge_unselect(GanvEdge* edge)
{
	ganv_edge_set_selected(edge, FALSE);
}

void
ganv_edge_highlight(GanvEdge* edge)
{
	ganv_edge_set_highlighted(edge, TRUE);
}

void
ganv_edge_unhighlight(GanvEdge* edge)
{
	ganv_edge_set_highlighted(edge, FALSE);
}

void
ganv_edge_set_highlighted(GanvEdge* edge, gboolean highlighted)
{
	edge->impl->highlighted = highlighted;
	ganv_edge_request_redraw(GANV_ITEM(edge), &edge->impl->coords);
}

void
ganv_edge_tick(GanvEdge* edge, double seconds)
{
	ganv_item_set(GANV_ITEM(edge),
	              "dash-offset", seconds * 8.0,
	              NULL);
}

void
ganv_edge_disconnect(GanvEdge* edge)
{
	if (!edge->impl->ghost) {
		ganv_canvas_disconnect_edge(
			GANV_CANVAS(edge->item.impl->canvas),
			edge);
	}
}

void
ganv_edge_remove(GanvEdge* edge)
{
	if (!edge->impl->ghost) {
		ganv_canvas_remove_edge(
			GANV_CANVAS(edge->item.impl->canvas),
			edge);
	}
}

GanvNode*
ganv_edge_get_tail(const GanvEdge* edge)
{
	return edge->impl->tail;
}

GanvNode*
ganv_edge_get_head(const GanvEdge* edge)
{
	return edge->impl->head;
}
