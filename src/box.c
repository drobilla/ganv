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

#include <math.h>
#include <string.h>

#include <cairo.h>

#include "ganv/box.h"

#include "./boilerplate.h"
#include "./color.h"
#include "./gettext.h"

static const double STACKED_OFFSET = 4.0;

G_DEFINE_TYPE(GanvBox, ganv_box, GANV_TYPE_NODE)

static GanvNodeClass* parent_class;

enum {
	PROP_0,
	PROP_X1,
	PROP_Y1,
	PROP_X2,
	PROP_Y2,
	PROP_RADIUS_TL,
	PROP_RADIUS_TR,
	PROP_RADIUS_BR,
	PROP_RADIUS_BL,
	PROP_STACKED
};

static void
ganv_box_init(GanvBox* box)
{
	memset(&box->coords, '\0', sizeof(GanvBoxCoords));
	box->coords.border_width = box->node.border_width;

	box->old_coords = box->coords;
	box->radius_tl  = 0.0;
	box->radius_tr  = 0.0;
	box->radius_br  = 0.0;
	box->radius_bl  = 0.0;
}

static void
ganv_box_destroy(GtkObject* object)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(GANV_IS_BOX(object));

	if (GTK_OBJECT_CLASS(parent_class)->destroy) {
		(*GTK_OBJECT_CLASS(parent_class)->destroy)(object);
	}
}

static void
ganv_box_set_property(GObject*      object,
                      guint         prop_id,
                      const GValue* value,
                      GParamSpec*   pspec)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(GANV_IS_BOX(object));

	GanvBox*       box    = GANV_BOX(object);
	GanvBoxCoords* coords = &box->coords;

	switch (prop_id) {
		SET_CASE(X1, double, coords->x1);
		SET_CASE(Y1, double, coords->y1);
		SET_CASE(X2, double, coords->x2);
		SET_CASE(Y2, double, coords->y2);
		SET_CASE(RADIUS_TL, double, box->radius_tl);
		SET_CASE(RADIUS_TR, double, box->radius_tr);
		SET_CASE(RADIUS_BR, double, box->radius_br);
		SET_CASE(RADIUS_BL, double, box->radius_bl);
		SET_CASE(STACKED, boolean, coords->stacked);
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
ganv_box_get_property(GObject*    object,
                      guint       prop_id,
                      GValue*     value,
                      GParamSpec* pspec)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(GANV_IS_BOX(object));

	GanvBox* box = GANV_BOX(object);

	switch (prop_id) {
		GET_CASE(X1, double, box->coords.x1);
		GET_CASE(X2, double, box->coords.x2);
		GET_CASE(Y1, double, box->coords.y1);
		GET_CASE(Y2, double, box->coords.y2);
		GET_CASE(RADIUS_TL, double, box->radius_tl);
		GET_CASE(RADIUS_TR, double, box->radius_tr);
		GET_CASE(RADIUS_BR, double, box->radius_br);
		GET_CASE(RADIUS_BL, double, box->radius_bl);
		GET_CASE(STACKED, boolean, box->coords.stacked);
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
ganv_box_bounds_item(const GanvBoxCoords* coords,
                     double* x1, double* y1,
                     double* x2, double* y2)
{
	*x1 = MIN(coords->x1, coords->x2) - coords->border_width;
	*y1 = MIN(coords->y1, coords->y2) - coords->border_width;
	*x2 = MAX(coords->x1, coords->x2) + coords->border_width;
	*y2 = MAX(coords->y1, coords->y2) + coords->border_width;

	if (coords->stacked) {
		*x2 += STACKED_OFFSET;
		*y2 += STACKED_OFFSET;
	}
}


static void
request_redraw(GnomeCanvasItem*     item,
               const GanvBoxCoords* coords,
               gboolean             world)
{
	double x1, y1, x2, y2;
	ganv_box_bounds_item(coords, &x1, &y1, &x2, &y2);

	if (!world) {
		// Convert from parent-relative coordinates to world coordinates
		gnome_canvas_item_i2w(item, &x1, &y1);
		gnome_canvas_item_i2w(item, &x2, &y2);
	}

	gnome_canvas_request_redraw(item->canvas, x1, y1, x2, y2);
}

static void
coords_i2w(GnomeCanvasItem* item, GanvBoxCoords* coords)
{
	gnome_canvas_item_i2w(item, &coords->x1, &coords->y1);
	gnome_canvas_item_i2w(item, &coords->x2, &coords->y2);
}

static void
ganv_box_bounds(GnomeCanvasItem* item,
                double* x1, double* y1,
                double* x2, double* y2)
{
	// Note this will not be correct if children are outside the box bounds
	GanvBox* box = GANV_BOX(item);
	ganv_box_bounds_item(&box->coords, x1, y1, x2, y2);
	gnome_canvas_item_i2w(item, x1, y1);
	gnome_canvas_item_i2w(item, x2, y2);
}

static void
ganv_box_update(GnomeCanvasItem* item,
                double*          affine,
                ArtSVP*          clip_path,
                int              flags)
{
	GanvBox* box = GANV_BOX(item);
	box->coords.border_width = box->node.border_width;

	// Request redraw of old location
	request_redraw(item, &box->old_coords, TRUE);

	GnomeCanvasItemClass* item_class = GNOME_CANVAS_ITEM_CLASS(parent_class);
	item_class->update(item, affine, clip_path, flags);

	// Store old coordinates in world relative coordinates in case the
	// group we are in moves between now and the next update
	box->old_coords = box->coords;
	coords_i2w(item, &box->old_coords);

	// Get bounding box
	double x1, x2, y1, y2;
	ganv_box_bounds(item, &x1, &y1, &x2, &y2);

	// Update item canvas coordinates
	gnome_canvas_w2c_d(GNOME_CANVAS(item->canvas), x1, y1, &item->x1, &item->y1);
	gnome_canvas_w2c_d(GNOME_CANVAS(item->canvas), x2, y2, &item->x2, &item->y2);

	// Request redraw of new location
	request_redraw(item, &box->coords, FALSE);

}

static void
ganv_box_render(GnomeCanvasItem* item,
                GnomeCanvasBuf*  buf)
{
	// Not implemented
}

static void
ganv_box_draw(GnomeCanvasItem* item,
              GdkDrawable* drawable,
              int cx, int cy,
              int width, int height)
{
	GanvBox* me = GANV_BOX(item);
	cairo_t*       cr = gdk_cairo_create(drawable);

	double x1 = me->coords.x1;
	double y1 = me->coords.y1;
	double x2 = me->coords.x2;
	double y2 = me->coords.y2;
	gnome_canvas_item_i2w(item, &x1, &y1);
	gnome_canvas_item_i2w(item, &x2, &y2);

	double dash_length, border_color, fill_color;
	ganv_node_get_draw_properties(
		&me->node, &dash_length, &border_color, &fill_color);

	double r, g, b, a;

	double degrees = M_PI / 180.0;

	for (int i = (me->coords.stacked ? 1 : 0); i >= 0; --i) {
		const int x = cx - (STACKED_OFFSET * i);
		const int y = cy - (STACKED_OFFSET * i);

		if (me->radius_tl == 0.0 && me->radius_tr == 0.0
		    && me->radius_br == 0.0 && me->radius_bl == 0.0) {
			// Simple rectangle
			cairo_rectangle(cr, x1 - x, y1 - y, x2 - x1, y2 - y1);
		} else {
			// Rounded rectangle
			cairo_new_sub_path(cr);
			cairo_arc(cr,
			          x2 - x - me->radius_tr,
			          y1 - y + me->radius_tr,
			          me->radius_tr, -90 * degrees, 0 * degrees);
			cairo_arc(cr,
			          x2 - x - me->radius_br, y2 - y - me->radius_br,
			          me->radius_br, 0 * degrees, 90 * degrees);
			cairo_arc(cr,
			          x1 - x + me->radius_bl, y2 - y - me->radius_bl,
			          me->radius_bl, 90 * degrees, 180 * degrees);
			cairo_arc(cr,
			          x1 - x + me->radius_tl, y1 - y + me->radius_tl,
			          me->radius_tl, 180 * degrees, 270 * degrees);
			cairo_close_path(cr);
		}

		// Fill
		color_to_rgba(fill_color, &r, &g, &b, &a);
		cairo_set_source_rgba(cr, r, g, b, a);
		cairo_fill_preserve(cr);

		// Border
		if (me->coords.border_width > 0.0) {
			color_to_rgba(border_color, &r, &g, &b, &a);
			cairo_set_source_rgba(cr, r, g, b, a);
			cairo_set_line_width(cr, me->coords.border_width);
			if (dash_length > 0) {
				cairo_set_dash(cr, &dash_length, 1, me->node.dash_offset);
			}
		}
		cairo_stroke(cr);
	}

	GnomeCanvasItemClass* item_class = GNOME_CANVAS_ITEM_CLASS(parent_class);
	item_class->draw(item, drawable, cx, cy, width, height);

	cairo_destroy(cr);
}

static double
ganv_box_point(GnomeCanvasItem* item,
               double x, double y,
               int cx, int cy,
               GnomeCanvasItem** actual_item)
{
	GanvBox* box = GANV_BOX(item);

	*actual_item = NULL;

	double x1, y1, x2, y2;
	ganv_box_bounds_item(&box->coords, &x1, &y1, &x2, &y2);

	// Point is inside the box (distance 0)
	if ((x >= x1) && (y >= y1) && (x <= x2) && (y <= y2)) {
		GnomeCanvasItemClass* item_class = GNOME_CANVAS_ITEM_CLASS(parent_class);
		double d = item_class->point(item, x, y, cx, cy, actual_item);
		if (*actual_item) {
			return d;
		} else {
			*actual_item = item;
			return 0.0;
		}
	}

	// Point is outside the box
	double dx, dy;

	// Find horizontal distance to nearest edge
	if (x < x1) {
		dx = x1 - x;
	} else if (x > x2) {
		dx = x - x2;
	} else {
		dx = 0.0;
	}

	// Find vertical distance to nearest edge
	if (y < y1) {
		dy = y1 - y;
	} else if (y > y2) {
		dy = y - y2;
	} else {
		dy = 0.0;
	}

	return sqrt((dx * dx) + (dy * dy));
}

static gboolean
ganv_box_is_within(const GanvNode* self,
                   double          x1,
                   double          y1,
                   double          x2,
                   double          y2)
{
	double bx1, by1, bx2, by2;
	g_object_get(G_OBJECT(self),
	             "x1", &bx1,
	             "y1", &by1,
	             "x2", &bx2,
	             "y2", &by2,
	             NULL);

	gnome_canvas_item_i2w(GNOME_CANVAS_ITEM(self), &bx1, &by1);
	gnome_canvas_item_i2w(GNOME_CANVAS_ITEM(self), &bx2, &by2);

	return (   bx1 >= x1
	           && by2 >= y1
	           && bx2 <= x2
	           && by2 <= y2);
}

static void
ganv_box_default_set_width(GanvBox* box,
                           double   width)
{
	gnome_canvas_item_set(GNOME_CANVAS_ITEM(box),
	                      "x2", ganv_box_get_x1(box) + width,
	                      NULL);
}

static void
ganv_box_default_set_height(GanvBox* box,
                            double         height)
{
	gnome_canvas_item_set(GNOME_CANVAS_ITEM(box),
	                      "y2", ganv_box_get_y1(box) + height,
	                      NULL);
}

static void
ganv_box_class_init(GanvBoxClass* class)
{
	GObjectClass*         gobject_class = (GObjectClass*)class;
	GtkObjectClass*       object_class  = (GtkObjectClass*)class;
	GnomeCanvasItemClass* item_class    = (GnomeCanvasItemClass*)class;
	GanvNodeClass*        node_class    = (GanvNodeClass*)class;

	parent_class = GANV_NODE_CLASS(g_type_class_peek_parent(class));

	gobject_class->set_property = ganv_box_set_property;
	gobject_class->get_property = ganv_box_get_property;

	g_object_class_install_property(
		gobject_class, PROP_X1, g_param_spec_double(
			"x1",
			_("x1"),
			_("top left x coordinate"),
			-G_MAXDOUBLE, G_MAXDOUBLE,
			0.0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_Y1, g_param_spec_double(
			"y1",
			_("y1"),
			_("top left y coordinate"),
			-G_MAXDOUBLE, G_MAXDOUBLE,
			0.0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_X2, g_param_spec_double(
			"x2",
			_("x2"),
			_("bottom right x coordinate"),
			-G_MAXDOUBLE, G_MAXDOUBLE,
			0.0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_Y2, g_param_spec_double(
			"y2",
			_("y2"),
			_("bottom right y coordinate"),
			-G_MAXDOUBLE, G_MAXDOUBLE,
			0.0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_RADIUS_TL, g_param_spec_double(
			"radius-tl",
			_("top left corner radius"),
			_("top left corner radius"),
			0.0, G_MAXDOUBLE,
			0.0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_RADIUS_TR, g_param_spec_double(
			"radius-tr",
			_("top right corner radius"),
			_("top right corner radius"),
			0.0, G_MAXDOUBLE,
			0.0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_RADIUS_BR, g_param_spec_double(
			"radius-br",
			_("bottom right corner radius"),
			_("bottom right corner radius"),
			0.0, G_MAXDOUBLE,
			0.0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_RADIUS_BL, g_param_spec_double(
			"radius-bl",
			_("bottom left corner radius"),
			_("bottom left corner radius"),
			0.0, G_MAXDOUBLE,
			0.0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_STACKED, g_param_spec_boolean(
			"stacked",
			_("stacked"),
			_("Show the box with a stacked appearance."),
			FALSE,
			G_PARAM_READWRITE));

	object_class->destroy = ganv_box_destroy;

	item_class->update = ganv_box_update;
	item_class->bounds = ganv_box_bounds;
	item_class->point  = ganv_box_point;
	item_class->render = ganv_box_render;
	item_class->draw   = ganv_box_draw;

	node_class->is_within = ganv_box_is_within;

	class->set_width  = ganv_box_default_set_width;
	class->set_height = ganv_box_default_set_height;
}

void
ganv_box_normalize(GanvBox* box)
{
	const double x1 = box->coords.x1;
	const double y1 = box->coords.y1;
	const double x2 = box->coords.x2;
	const double y2 = box->coords.y2;

	box->coords.x1 = MIN(x1, x2);
	box->coords.y1 = MIN(y1, y2);
	box->coords.x2 = MAX(x1, x2);
	box->coords.y2 = MAX(y1, y2);
}
