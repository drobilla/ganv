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

#include "ganv/circle.h"

#include "./color.h"
#include "./boilerplate.h"
#include "./gettext.h"

G_DEFINE_TYPE(GanvCircle, ganv_circle, GANV_TYPE_NODE)

static GanvNodeClass* parent_class;

enum {
	PROP_0,
	PROP_RADIUS
};

static void
ganv_circle_init(GanvCircle* circle)
{
	memset(&circle->coords, '\0', sizeof(GanvCircleCoords));
	circle->coords.radius = 8.0;
	circle->coords.width  = 2.0;
	circle->old_coords    = circle->coords;
}

static void
ganv_circle_destroy(GtkObject* object)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(GANV_IS_CIRCLE(object));

	if (GTK_OBJECT_CLASS(parent_class)->destroy) {
		(*GTK_OBJECT_CLASS(parent_class)->destroy)(object);
	}
}

static void
ganv_circle_set_property(GObject*      object,
                         guint         prop_id,
                         const GValue* value,
                         GParamSpec*   pspec)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(GANV_IS_CIRCLE(object));

	GanvCircle* circle = GANV_CIRCLE(object);

	switch (prop_id) {
		SET_CASE(RADIUS, double, circle->coords.radius);
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
ganv_circle_get_property(GObject*    object,
                         guint       prop_id,
                         GValue*     value,
                         GParamSpec* pspec)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(GANV_IS_CIRCLE(object));

	GanvCircle* circle = GANV_CIRCLE(object);

	switch (prop_id) {
		GET_CASE(RADIUS, double, circle->coords.radius);
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static gboolean
ganv_circle_is_within(const GanvNode* self,
                      double                x1,
                      double                y1,
                      double                x2,
                      double                y2)
{
	double x, y;
	g_object_get(G_OBJECT(self), "x", &x, "y", &y, NULL);

	return x >= x1
		&& x <= x2
		&& y >= y1
		&& y <= y2;
}

static void
ganv_circle_tail_vector(const GanvNode* self,
                        const GanvNode* head,
                        double*         x,
                        double*         y,
                        double*         dx,
                        double*         dy)
{
	g_object_get(G_OBJECT(self), "x", x, "y", y, NULL);
	gnome_canvas_item_i2w(GNOME_CANVAS_ITEM(self)->parent, x, y);
	*dx = 0.0;
	*dy = 0.0;
}

static void
ganv_circle_head_vector(const GanvNode* self,
                        const GanvNode* tail,
                        double*         x,
                        double*         y,
                        double*         dx,
                        double*         dy)
{
	GanvCircle* circle = GANV_CIRCLE(self);

	double cx, cy;
	g_object_get(G_OBJECT(self), "x", &cx, "y", &cy, NULL);

	// FIXME: Not quite right, should be the connect point
	double tail_x, tail_y;
	g_object_get(G_OBJECT(tail),
	             "x", &tail_x,
	             "y", &tail_y,
	             NULL);

	const double xdist = tail_x - cx;
	const double ydist = tail_y - cy;
	const double h     = sqrt((xdist * xdist) + (ydist * ydist));
	const double theta = asin(xdist / (h + DBL_EPSILON));
	const double y_mod = (cy < tail_y) ? 1 : -1;
	const double ret_h = h - circle->coords.radius;
	const double ret_x = tail_x - sin(theta) * ret_h;
	const double ret_y = tail_y - cos(theta) * ret_h * y_mod;

	*x  = ret_x;
	*y  = ret_y;
	*dx = 0.0;
	*dy = 0.0;

	gnome_canvas_item_i2w(GNOME_CANVAS_ITEM(self)->parent, x, y);
}

static void
request_redraw(GnomeCanvasItem*        item,
               const GanvCircleCoords* coords,
               gboolean                world)
{
	const double w  = coords->width;

	double x1 = coords->x - coords->radius - w;
	double y1 = coords->y - coords->radius - w;
	double x2 = coords->x + coords->radius + w;
	double y2 = coords->y + coords->radius + w;

	if (!world) {
		// Convert from parent-relative coordinates to world coordinates
		gnome_canvas_item_i2w(item, &x1, &y1);
		gnome_canvas_item_i2w(item, &x2, &y2);
	}

	gnome_canvas_request_redraw(item->canvas, x1, y1, x2, y2);
	gnome_canvas_request_redraw(item->canvas, 0, 0, 10000, 10000);
}

static void
coords_i2w(GnomeCanvasItem* item, GanvCircleCoords* coords)
{
	gnome_canvas_item_i2w(item, &coords->x, &coords->y);
}

static void
ganv_circle_bounds_item(GnomeCanvasItem* item,
                        double* x1, double* y1,
                        double* x2, double* y2)
{
	const GanvCircle*       circle = GANV_CIRCLE(item);
	const GanvCircleCoords* coords = &circle->coords;
	*x1 = coords->x - coords->radius - coords->width;
	*y1 = coords->y - coords->radius - coords->width;
	*x2 = coords->x + coords->radius + coords->width;
	*y2 = coords->y + coords->radius + coords->width;
}

static void
ganv_circle_bounds(GnomeCanvasItem* item,
                   double* x1, double* y1,
                   double* x2, double* y2)
{
	ganv_circle_bounds_item(item, x1, y1, x2, y2);
	gnome_canvas_item_i2w(item, x1, y1);
	gnome_canvas_item_i2w(item, x2, y2);
}

static void
ganv_circle_update(GnomeCanvasItem* item,
                   double*          affine,
                   ArtSVP*          clip_path,
                   int              flags)
{
	GanvCircle* circle = GANV_CIRCLE(item);

	GnomeCanvasItemClass* item_class = GNOME_CANVAS_ITEM_CLASS(parent_class);
	if (item_class->update) {
		(*item_class->update)(item, affine, clip_path, flags);
	}

	// Request redraw of old location
	request_redraw(item, &circle->old_coords, TRUE);

	// Store old coordinates in world relative coordinates in case the
	// group we are in moves between now and the next update
	circle->old_coords = circle->coords;
	coords_i2w(item, &circle->old_coords);

	// Get bounding circle
	double x1, x2, y1, y2;
	ganv_circle_bounds(item, &x1, &y1, &x2, &y2);

	// Update item canvas coordinates
	gnome_canvas_w2c_d(GNOME_CANVAS(item->canvas), x1, y1, &item->x1, &item->y1);
	gnome_canvas_w2c_d(GNOME_CANVAS(item->canvas), x2, y2, &item->x2, &item->y2);

	// Request redraw of new location
	request_redraw(item, &circle->coords, FALSE);
}

static void
ganv_circle_render(GnomeCanvasItem* item,
                   GnomeCanvasBuf*  buf)
{
	// Not implemented
}

static void
ganv_circle_draw(GnomeCanvasItem* item,
                 GdkDrawable* drawable,
                 int x, int y,
                 int width, int height)
{
	GanvCircle* me = GANV_CIRCLE(item);
	cairo_t*          cr = gdk_cairo_create(drawable);

	double r, g, b, a;

	double cx = me->coords.x;
	double cy = me->coords.y;
	gnome_canvas_item_i2w(item, &cx, &cy);

	double dash_length, border_color, fill_color;
	ganv_node_get_draw_properties(
		&me->node, &dash_length, &border_color, &fill_color);

	cairo_arc(cr,
	          cx - x,
	          cy - y,
	          me->coords.radius,
	          0, 2 * M_PI);

	// Fill
	color_to_rgba(fill_color, &r, &g, &b, &a);
	cairo_set_source_rgba(cr, r, g, b, a);
	cairo_fill_preserve(cr);

	// Border
	color_to_rgba(border_color, &r, &g, &b, &a);
	cairo_set_source_rgba(cr, r, g, b, a);
	cairo_set_line_width(cr, me->coords.width);
	if (dash_length > 0) {
		cairo_set_dash(cr, &dash_length, 1, me->node.dash_offset);
	}
	cairo_stroke(cr);

	cairo_destroy(cr);
}

static double
ganv_circle_point(GnomeCanvasItem* item,
                  double x, double y,
                  int cx, int cy,
                  GnomeCanvasItem** actual_item)
{
	const GanvCircle*       circle = GANV_CIRCLE(item);
	const GanvCircleCoords* coords = &circle->coords;

	*actual_item = item;

	const double dx = fabs(x - coords->x);
	const double dy = fabs(y - coords->y);
	const double d  = sqrt((dx * dx) + (dy * dy));

	if (d <= coords->radius + coords->width) {
		// Point is inside the circle
		return 0.0;
	} else {
		// Distance from the edge of the circle
		return d - (coords->radius + coords->width);
	}
}

static void
ganv_circle_class_init(GanvCircleClass* class)
{
	GObjectClass*         gobject_class = (GObjectClass*)class;
	GtkObjectClass*       object_class  = (GtkObjectClass*)class;
	GnomeCanvasItemClass* item_class    = (GnomeCanvasItemClass*)class;
	GanvNodeClass*        node_class    = (GanvNodeClass*)class;

	parent_class = GANV_NODE_CLASS(g_type_class_peek_parent(class));

	gobject_class->set_property = ganv_circle_set_property;
	gobject_class->get_property = ganv_circle_get_property;

	g_object_class_install_property(
		gobject_class, PROP_RADIUS, g_param_spec_double(
			"radius",
			_("radius"),
			_("radius of circle"),
			0, G_MAXDOUBLE,
			0.0,
			G_PARAM_READWRITE));

	object_class->destroy = ganv_circle_destroy;

	node_class->is_within   = ganv_circle_is_within;
	node_class->tail_vector = ganv_circle_tail_vector;
	node_class->head_vector = ganv_circle_head_vector;

	item_class->update = ganv_circle_update;
	item_class->bounds = ganv_circle_bounds;
	item_class->point  = ganv_circle_point;
	item_class->render = ganv_circle_render;
	item_class->draw   = ganv_circle_draw;
}
