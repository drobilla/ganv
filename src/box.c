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

#include <math.h>
#include <string.h>

#include <cairo.h>

#include "ganv/box.h"

#include "./boilerplate.h"
#include "./color.h"
#include "./gettext.h"
#include "./ganv-private.h"

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
	box->impl = G_TYPE_INSTANCE_GET_PRIVATE(
		box, GANV_TYPE_BOX, GanvBoxImpl);

	memset(&box->impl->coords, '\0', sizeof(GanvBoxCoords));
	box->impl->coords.border_width = 0.0;

	box->impl->old_coords = box->impl->coords;
	box->impl->radius_tl  = 0.0;
	box->impl->radius_tr  = 0.0;
	box->impl->radius_br  = 0.0;
	box->impl->radius_bl  = 0.0;
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
	GanvBoxImpl*   impl   = box->impl;
	GanvBoxCoords* coords = &impl->coords;

	switch (prop_id) {
		SET_CASE(X1, double, coords->x1);
		SET_CASE(Y1, double, coords->y1);
		SET_CASE(X2, double, coords->x2);
		SET_CASE(Y2, double, coords->y2);
		SET_CASE(RADIUS_TL, double, impl->radius_tl);
		SET_CASE(RADIUS_TR, double, impl->radius_tr);
		SET_CASE(RADIUS_BR, double, impl->radius_br);
		SET_CASE(RADIUS_BL, double, impl->radius_bl);
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

	GanvBox*       box    = GANV_BOX(object);
	GanvBoxImpl*   impl   = box->impl;
	GanvBoxCoords* coords = &impl->coords;

	switch (prop_id) {
		GET_CASE(X1, double, coords->x1);
		GET_CASE(X2, double, coords->x2);
		GET_CASE(Y1, double, coords->y1);
		GET_CASE(Y2, double, coords->y2);
		GET_CASE(RADIUS_TL, double, impl->radius_tl);
		GET_CASE(RADIUS_TR, double, impl->radius_tr);
		GET_CASE(RADIUS_BR, double, impl->radius_br);
		GET_CASE(RADIUS_BL, double, impl->radius_bl);
		GET_CASE(STACKED, boolean, impl->coords.stacked);
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
	*x1 = coords->x1 - coords->border_width;
	*y1 = coords->y1 - coords->border_width;
	*x2 = coords->x2 + coords->border_width + (coords->stacked * STACKED_OFFSET);
	*y2 = coords->y2 + coords->border_width + (coords->stacked * STACKED_OFFSET);
}

void
ganv_box_request_redraw(GanvItem*            item,
                        const GanvBoxCoords* coords,
                        gboolean             world)
{
	double x1, y1, x2, y2;
	ganv_box_bounds_item(coords, &x1, &y1, &x2, &y2);

	if (!world) {
		// Convert from item-relative coordinates to world coordinates
		ganv_item_i2w_pair(item, &x1, &y1, &x2, &y2);
	}

	ganv_canvas_request_redraw_w(item->canvas, x1, y1, x2, y2);
}

static void
coords_i2w(GanvItem* item, GanvBoxCoords* coords)
{
	ganv_item_i2w_pair(item, &coords->x1, &coords->y1, &coords->x2, &coords->y2);
}

static void
ganv_box_bounds(GanvItem* item,
                double* x1, double* y1,
                double* x2, double* y2)
{
	// Note this will not be correct if children are outside the box bounds
	GanvBox* box = (GanvBox*)item;
	ganv_box_bounds_item(&box->impl->coords, x1, y1, x2, y2);
}

static void
ganv_box_update(GanvItem* item, int flags)
{
	GanvBox*     box  = GANV_BOX(item);
	GanvBoxImpl* impl = box->impl;
	impl->coords.border_width = box->node.impl->border_width;

	// Request redraw of old location
	ganv_box_request_redraw(item, &impl->old_coords, TRUE);

	GanvItemClass* item_class = GANV_ITEM_CLASS(parent_class);
	item_class->update(item, flags);

	// Store old coordinates in world relative coordinates in case the
	// group we are in moves between now and the next update
	impl->old_coords = impl->coords;
	coords_i2w(item, &impl->old_coords);

	ganv_box_normalize(box);

	// Update world-relative bounding box
	ganv_box_bounds(item, &item->x1, &item->y1, &item->x2, &item->y2);
	ganv_item_i2w_pair(item, &item->x1, &item->y1, &item->x2, &item->y2);

	// Request redraw of new location
	ganv_box_request_redraw(item, &impl->coords, FALSE);
}

static void
ganv_box_draw(GanvItem* item,
              cairo_t* cr, double cx, double cy, double cw, double ch)
{
	GanvBox*     box  = GANV_BOX(item);
	GanvBoxImpl* impl = box->impl;

	double x1 = impl->coords.x1;
	double y1 = impl->coords.y1;
	double x2 = impl->coords.x2;
	double y2 = impl->coords.y2;
	ganv_item_i2w_pair(item, &x1, &y1, &x2, &y2);

	double dash_length, border_color, fill_color;
	ganv_node_get_draw_properties(
		&box->node, &dash_length, &border_color, &fill_color);

	double r, g, b, a;

	static const double degrees = G_PI / 180.0;

	for (int i = (impl->coords.stacked ? 1 : 0); i >= 0; --i) {
		const double x = 0.0 - (STACKED_OFFSET * i);
		const double y = 0.0 - (STACKED_OFFSET * i);

		if (impl->radius_tl == 0.0 && impl->radius_tr == 0.0
		    && impl->radius_br == 0.0 && impl->radius_bl == 0.0) {
			// Simple rectangle
			cairo_rectangle(cr, x1 - x, y1 - y, x2 - x1, y2 - y1);
		} else {
			// Rounded rectangle
			cairo_new_sub_path(cr);
			cairo_arc(cr,
			          x2 - x - impl->radius_tr,
			          y1 - y + impl->radius_tr,
			          impl->radius_tr, -90 * degrees, 0 * degrees);
			cairo_arc(cr,
			          x2 - x - impl->radius_br, y2 - y - impl->radius_br,
			          impl->radius_br, 0 * degrees, 90 * degrees);
			cairo_arc(cr,
			          x1 - x + impl->radius_bl, y2 - y - impl->radius_bl,
			          impl->radius_bl, 90 * degrees, 180 * degrees);
			cairo_arc(cr,
			          x1 - x + impl->radius_tl, y1 - y + impl->radius_tl,
			          impl->radius_tl, 180 * degrees, 270 * degrees);
			cairo_close_path(cr);
		}

		// Fill
		color_to_rgba(fill_color, &r, &g, &b, &a);
		cairo_set_source_rgba(cr, r, g, b, a);
		cairo_fill_preserve(cr);

		// Border
		if (impl->coords.border_width > 0.0) {
			color_to_rgba(border_color, &r, &g, &b, &a);
			cairo_set_source_rgba(cr, r, g, b, a);
			cairo_set_line_width(cr, impl->coords.border_width);
			if (dash_length > 0) {
				cairo_set_dash(cr, &dash_length, 1, box->node.impl->dash_offset);
			} else {
				cairo_set_dash(cr, &dash_length, 0, 0);
			}
		}
		cairo_stroke(cr);
	}

	GanvItemClass* item_class = GANV_ITEM_CLASS(parent_class);
	item_class->draw(item, cr, cx, cy, cw, ch);
}

static double
ganv_box_point(GanvItem* item, double x, double y, GanvItem** actual_item)
{
	GanvBox*     box  = GANV_BOX(item);
	GanvBoxImpl* impl = box->impl;

	*actual_item = NULL;

	double x1, y1, x2, y2;
	ganv_box_bounds_item(&impl->coords, &x1, &y1, &x2, &y2);

	// Point is inside the box (distance 0)
	if ((x >= x1) && (y >= y1) && (x <= x2) && (y <= y2)) {
		*actual_item = item;
		return 0.0;
	}

	// Point is outside the box
	double dx = 0.0;
	double dy = 0.0;

	// Find horizontal distance to nearest edge
	if (x < x1) {
		dx = x1 - x;
	} else if (x > x2) {
		dx = x - x2;
	}

	// Find vertical distance to nearest edge
	if (y < y1) {
		dy = y1 - y;
	} else if (y > y2) {
		dy = y - y2;
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

	ganv_item_i2w_pair(GANV_ITEM(self), &bx1, &by1, &bx2, &by2);

	return (   bx1 >= x1
	           && by2 >= y1
	           && bx2 <= x2
	           && by2 <= y2);
}

static void
ganv_box_default_set_width(GanvBox* box,
                           double   width)
{
	ganv_item_set(GANV_ITEM(box),
	              "x2", ganv_box_get_x1(box) + width,
	              NULL);
}

static void
ganv_box_default_set_height(GanvBox* box,
                            double   height)
{
	ganv_item_set(GANV_ITEM(box),
	              "y2", ganv_box_get_y1(box) + height,
	              NULL);
}

static void
ganv_box_class_init(GanvBoxClass* klass)
{
	GObjectClass*   gobject_class = (GObjectClass*)klass;
	GtkObjectClass* object_class  = (GtkObjectClass*)klass;
	GanvItemClass*  item_class    = (GanvItemClass*)klass;
	GanvNodeClass*  node_class    = (GanvNodeClass*)klass;

	parent_class = GANV_NODE_CLASS(g_type_class_peek_parent(klass));

	g_type_class_add_private(klass, sizeof(GanvBoxImpl));

	gobject_class->set_property = ganv_box_set_property;
	gobject_class->get_property = ganv_box_get_property;

	g_object_class_install_property(
		gobject_class, PROP_X1, g_param_spec_double(
			"x1",
			_("x1"),
			_("Top left x coordinate."),
			-G_MAXDOUBLE, G_MAXDOUBLE,
			0.0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_Y1, g_param_spec_double(
			"y1",
			_("y1"),
			_("Top left y coordinate."),
			-G_MAXDOUBLE, G_MAXDOUBLE,
			0.0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_X2, g_param_spec_double(
			"x2",
			_("x2"),
			_("Bottom right x coordinate."),
			-G_MAXDOUBLE, G_MAXDOUBLE,
			0.0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_Y2, g_param_spec_double(
			"y2",
			_("y2"),
			_("Bottom right y coordinate."),
			-G_MAXDOUBLE, G_MAXDOUBLE,
			0.0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_RADIUS_TL, g_param_spec_double(
			"radius-tl",
			_("Top left radius"),
			_("The radius of the top left corner."),
			0.0, G_MAXDOUBLE,
			0.0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_RADIUS_TR, g_param_spec_double(
			"radius-tr",
			_("Top right radius"),
			_("The radius of the top right corner."),
			0.0, G_MAXDOUBLE,
			0.0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_RADIUS_BR, g_param_spec_double(
			"radius-br",
			_("Bottom right radius"),
			_("The radius of the bottom right corner."),
			0.0, G_MAXDOUBLE,
			0.0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_RADIUS_BL, g_param_spec_double(
			"radius-bl",
			_("Bottom left radius"),
			_("The radius of the bottom left corner."),
			0.0, G_MAXDOUBLE,
			0.0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_STACKED, g_param_spec_boolean(
			"stacked",
			_("Stacked"),
			_("Whether to show the box with a stacked appearance."),
			FALSE,
			G_PARAM_READWRITE));

	object_class->destroy = ganv_box_destroy;

	item_class->update = ganv_box_update;
	item_class->bounds = ganv_box_bounds;
	item_class->point  = ganv_box_point;
	item_class->draw   = ganv_box_draw;

	node_class->is_within = ganv_box_is_within;

	klass->set_width  = ganv_box_default_set_width;
	klass->set_height = ganv_box_default_set_height;
}

void
ganv_box_normalize(GanvBox* box)
{
	if (box->impl->coords.x2 < box->impl->coords.x1) {
		const double tmp = box->impl->coords.x1;
		box->impl->coords.x1 = box->impl->coords.x2;
		box->impl->coords.x2 = tmp;
	}
	if (box->impl->coords.y2 < box->impl->coords.y1) {
		const double tmp = box->impl->coords.y1;
		box->impl->coords.y1 = box->impl->coords.y2;
		box->impl->coords.y2 = tmp;
	}
}

double
ganv_box_get_x1(const GanvBox* box)
{
	return box->impl->coords.x1;
}

double
ganv_box_get_y1(const GanvBox* box)
{
	return box->impl->coords.y1;
}

double
ganv_box_get_x2(const GanvBox* box)
{
	return box->impl->coords.x2;
}

double
ganv_box_get_y2(const GanvBox* box)
{
	return box->impl->coords.y2;
}

double
ganv_box_get_width(const GanvBox* box)
{
	return box->impl->coords.x2 - box->impl->coords.x1;
}

void
ganv_box_set_width(GanvBox* box,
                   double   width)
{
	GANV_BOX_GET_CLASS(box)->set_width(box, width);
}

double
ganv_box_get_height(const GanvBox* box)
{
	return box->impl->coords.y2 - box->impl->coords.y1;
}

void
ganv_box_set_height(GanvBox* box,
                    double   height)
{
	GANV_BOX_GET_CLASS(box)->set_height(box, height);
}

double
ganv_box_get_border_width(const GanvBox* box)
{
	return box->impl->coords.border_width;
}
