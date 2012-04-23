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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtkstyle.h>

#include "ganv/canvas-base.h"
#include "ganv/canvas.h"
#include "ganv/text.h"

#include "./color.h"
#include "./boilerplate.h"
#include "./gettext.h"
#include "./ganv-private.h"

G_DEFINE_TYPE(GanvText, ganv_text, GANV_TYPE_ITEM)

static GanvItemClass* parent_class;

enum {
	PROP_0,
	PROP_TEXT,
	PROP_X,
	PROP_Y,
	PROP_WIDTH,
	PROP_HEIGHT,
	PROP_COLOR
};

static void
ganv_text_init(GanvText* text)
{
	GanvTextImpl* impl = G_TYPE_INSTANCE_GET_PRIVATE(
		text, GANV_TYPE_TEXT, GanvTextImpl);

	text->impl = impl;

	memset(&impl->coords, '\0', sizeof(GanvTextCoords));
	impl->coords.width  = 1.0;
	impl->coords.height = 1.0;
	impl->old_coords = impl->coords;

	impl->surface      = NULL;
	impl->text         = NULL;
	impl->color        = 0xFFFFFFFF;
	impl->needs_layout = FALSE;
}

static void
ganv_text_destroy(GtkObject* object)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(GANV_IS_TEXT(object));

	GanvText*     text = GANV_TEXT(object);
	GanvTextImpl* impl = text->impl;

	if (impl->text) {
		g_free(impl->text);
		impl->text = NULL;
	}

	if (impl->surface) {
		cairo_surface_destroy(impl->surface);
		impl->surface = NULL;
	}

	if (GTK_OBJECT_CLASS(parent_class)->destroy) {
		(*GTK_OBJECT_CLASS(parent_class)->destroy)(object);
	}
}

static void
ganv_text_layout(GanvText* text)
{
	GanvTextImpl* impl      = text->impl;
	GanvItem*     item      = GANV_ITEM(text);
	GanvCanvas*   canvas    = GANV_CANVAS(item->canvas);
	GtkWidget*    widget    = GTK_WIDGET(canvas);
	double        font_size = ganv_canvas_get_font_size(canvas);
	guint         color     = 0xFFFFFFFF;

	GtkStyle*             style   = gtk_rc_get_style(widget);
	PangoFontDescription* font    = pango_font_description_copy(style->font_desc);
	PangoLayout*          layout  = gtk_widget_create_pango_layout(widget, impl->text);
	PangoContext*         context = pango_layout_get_context(layout);
	cairo_font_options_t* options = cairo_font_options_copy(
		pango_cairo_context_get_font_options(context));

	pango_font_description_set_size(font, font_size * (double)PANGO_SCALE);
	pango_layout_set_font_description(layout, font);

	if (cairo_font_options_get_antialias(options) == CAIRO_ANTIALIAS_SUBPIXEL) {
		cairo_font_options_set_antialias(options, CAIRO_ANTIALIAS_GRAY);
	}

	pango_cairo_context_set_font_options(context, options);
	cairo_font_options_destroy(options);

	int width, height;
	pango_layout_get_pixel_size(layout, &width, &height);

	impl->coords.width = width;
	impl->coords.height = height;

	if (impl->surface) {
		cairo_surface_destroy(impl->surface);
	}

	impl->surface = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, width, height);

	cairo_t* cr = cairo_create(impl->surface);

	double r, g, b, a;
	color_to_rgba(color, &r, &g, &b, &a);

	cairo_set_source_rgba(cr, r, g, b, a);
	cairo_move_to(cr, 0, 0);
	pango_cairo_show_layout(cr, layout);

	cairo_destroy(cr);
	g_object_unref(layout);
	pango_font_description_free(font);

	impl->needs_layout = FALSE;
	ganv_item_request_update(GANV_ITEM(text));
}

static void
ganv_text_set_property(GObject*      object,
                       guint         prop_id,
                       const GValue* value,
                       GParamSpec*   pspec)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(GANV_IS_TEXT(object));

	GanvText*     text = GANV_TEXT(object);
	GanvTextImpl* impl = text->impl;

	switch (prop_id) {
		SET_CASE(X, double, impl->coords.x);
		SET_CASE(Y, double, impl->coords.y);
		SET_CASE(WIDTH, double, impl->coords.width);
		SET_CASE(HEIGHT, double, impl->coords.height);
		SET_CASE(COLOR, uint, impl->color)
	case PROP_TEXT:
		free(impl->text);
		impl->text = g_value_dup_string(value);
		impl->needs_layout = TRUE;
		if (GANV_IS_NODE(GANV_ITEM(text)->parent)) {
			ganv_node_resize(GANV_NODE(GANV_ITEM(text)->parent));
		}
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
ganv_text_get_property(GObject*    object,
                       guint       prop_id,
                       GValue*     value,
                       GParamSpec* pspec)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(GANV_IS_TEXT(object));

	GanvText*     text = GANV_TEXT(object);
	GanvTextImpl* impl = text->impl;

	if (impl->needs_layout && (prop_id == PROP_WIDTH
	                           || prop_id == PROP_HEIGHT)) {
		ganv_text_layout(text);
	}

	switch (prop_id) {
		GET_CASE(TEXT, string, impl->text);
		GET_CASE(X, double, impl->coords.x);
		GET_CASE(Y, double, impl->coords.y);
		GET_CASE(WIDTH, double, impl->coords.width);
		GET_CASE(HEIGHT, double, impl->coords.height);
		GET_CASE(COLOR, uint, impl->color);
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
ganv_text_bounds_item(GanvItem* item,
                      double* x1, double* y1,
                      double* x2, double* y2)
{
	GanvText*     text = GANV_TEXT(item);
	GanvTextImpl* impl = text->impl;

	if (impl->needs_layout) {
		ganv_text_layout(text);
	}

	*x1 = MIN(impl->coords.x, impl->coords.x + impl->coords.width);
	*y1 = MIN(impl->coords.y, impl->coords.y + impl->coords.height);
	*x2 = MAX(impl->coords.x, impl->coords.x + impl->coords.width);
	*y2 = MAX(impl->coords.y, impl->coords.y + impl->coords.height);
}

static void
ganv_text_bounds(GanvItem* item,
                 double* x1, double* y1,
                 double* x2, double* y2)
{
	ganv_text_bounds_item(item, x1, y1, x2, y2);
	ganv_item_i2w(item->parent, x1, y1);
	ganv_item_i2w(item->parent, x2, y2);
}

static void
ganv_text_update(GanvItem* item, int flags)
{
	double x1, y1, x2, y2;
	ganv_text_bounds(item, &x1, &y1, &x2, &y2);

	// I have no idea why this is necessary
	item->x1 = x1;
	item->y1 = y1;
	item->x2 = x2;
	item->y2 = y2;

	parent_class->update(item, flags);

	ganv_canvas_base_request_redraw(item->canvas, x1, y1, x2, y2);
}

static double
ganv_text_point(GanvItem* item,
                double x, double y,
                int cx, int cy,
                GanvItem** actual_item)
{
	*actual_item = NULL;

	double x1, y1, x2, y2;
	ganv_text_bounds_item(item, &x1, &y1, &x2, &y2);
	if ((x >= x1) && (y >= y1) && (x <= x2) && (y <= y2)) {
		return 0.0;
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

static void
ganv_text_draw(GanvItem* item,
               cairo_t* cr,
               int x, int y,
               int width, int height)
{
	GanvText*     text = GANV_TEXT(item);
	GanvTextImpl* impl = text->impl;

	double wx = impl->coords.x;
	double wy = impl->coords.y;
	ganv_item_i2w(item, &wx, &wy);

	// Round to the nearest pixel so text isn't blurry
	wx = lrint(wx);
	wy = lrint(wy);

	cairo_set_source_surface(cr, impl->surface, wx, wy);
	cairo_paint(cr);
}

static void
ganv_text_class_init(GanvTextClass* class)
{
	GObjectClass*   gobject_class = (GObjectClass*)class;
	GtkObjectClass* object_class  = (GtkObjectClass*)class;
	GanvItemClass*  item_class    = (GanvItemClass*)class;

	parent_class = GANV_ITEM_CLASS(g_type_class_peek_parent(class));

	g_type_class_add_private(class, sizeof(GanvTextImpl));

	gobject_class->set_property = ganv_text_set_property;
	gobject_class->get_property = ganv_text_get_property;

	g_object_class_install_property(
		gobject_class, PROP_TEXT, g_param_spec_string(
			"text",
			_("Text"),
			_("The string to display."),
			NULL,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_X, g_param_spec_double(
			"x",
			_("x"),
			_("Top left x coordinate."),
			-G_MAXDOUBLE, G_MAXDOUBLE,
			0.0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_Y, g_param_spec_double(
			"y",
			_("y"),
			_("Top left y coordinate."),
			-G_MAXDOUBLE, G_MAXDOUBLE,
			0.0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_WIDTH, g_param_spec_double(
			"width",
			_("Width"),
			_("The current width of the text."),
			-G_MAXDOUBLE, G_MAXDOUBLE,
			1.0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_HEIGHT, g_param_spec_double(
			"height",
			_("Height"),
			_("The current height of the text."),
			-G_MAXDOUBLE, G_MAXDOUBLE,
			1.0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_COLOR, g_param_spec_uint(
			"color",
			_("Color"),
			_("The color of the text."),
			0, G_MAXUINT,
			DEFAULT_TEXT_COLOR,
			G_PARAM_READWRITE));

	object_class->destroy = ganv_text_destroy;

	item_class->update = ganv_text_update;
	item_class->bounds = ganv_text_bounds;
	item_class->point  = ganv_text_point;
	item_class->draw   = ganv_text_draw;
}
