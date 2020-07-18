/* This file is part of Ganv.
 * Copyright 2007-2015 David Robillard <http://drobilla.net>
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

#include "ganv/canvas.h"
#include "ganv/text.h"

#include "./color.h"
#include "./boilerplate.h"
#include "./gettext.h"
#include "./ganv-private.h"

G_DEFINE_TYPE_WITH_CODE(GanvText, ganv_text, GANV_TYPE_ITEM,
                        G_ADD_PRIVATE(GanvText))

static GanvItemClass* parent_class;

enum {
	PROP_0,
	PROP_TEXT,
	PROP_X,
	PROP_Y,
	PROP_WIDTH,
	PROP_HEIGHT,
	PROP_COLOR,
	PROP_FONT_SIZE
};

static void
ganv_text_init(GanvText* text)
{
	GanvTextPrivate* impl =
	    (GanvTextPrivate*)ganv_text_get_instance_private(text);

	text->impl = impl;

	memset(&impl->coords, '\0', sizeof(GanvTextCoords));
	impl->coords.width  = 1.0;
	impl->coords.height = 1.0;
	impl->old_coords    = impl->coords;

	impl->layout       = NULL;
	impl->text         = NULL;
	impl->font_size    = 0.0;
	impl->color        = DEFAULT_TEXT_COLOR;
	impl->needs_layout = FALSE;
}

static void
ganv_text_destroy(GtkObject* object)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(GANV_IS_TEXT(object));

	GanvText*        text = GANV_TEXT(object);
	GanvTextPrivate* impl = text->impl;

	if (impl->text) {
		g_free(impl->text);
		impl->text = NULL;
	}

	if (impl->layout) {
		g_object_unref(impl->layout);
		impl->layout = NULL;
	}

	if (GTK_OBJECT_CLASS(parent_class)->destroy) {
		(*GTK_OBJECT_CLASS(parent_class)->destroy)(object);
	}
}

void
ganv_text_layout(GanvText* text)
{
	GanvTextPrivate* impl   = text->impl;
	GanvItem*        item   = GANV_ITEM(text);
	GanvCanvas*      canvas = ganv_item_get_canvas(item);
	GtkWidget*       widget = GTK_WIDGET(canvas);
	double           points = impl->font_size;
	GtkStyle*        style  = gtk_rc_get_style(widget);

	if (impl->font_size == 0.0) {
		points = ganv_canvas_get_font_size(canvas);
	}

	if (impl->layout) {
		g_object_unref(impl->layout);
	}
	impl->layout = gtk_widget_create_pango_layout(widget, impl->text);

	PangoFontDescription* font = pango_font_description_copy(style->font_desc);
	PangoContext*         ctx  = pango_layout_get_context(impl->layout);
	cairo_font_options_t* opt  = cairo_font_options_copy(
		pango_cairo_context_get_font_options(ctx));

	pango_font_description_set_size(font, points * (double)PANGO_SCALE);
	pango_layout_set_font_description(impl->layout, font);
	pango_cairo_context_set_font_options(ctx, opt);
	cairo_font_options_destroy(opt);
	pango_font_description_free(font);

	int width, height;
	pango_layout_get_pixel_size(impl->layout, &width, &height);

	impl->coords.width  = width;
	impl->coords.height = height;
	impl->needs_layout  = FALSE;

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

	GanvText*        text = GANV_TEXT(object);
	GanvTextPrivate* impl = text->impl;

	switch (prop_id) {
	case PROP_X:
		impl->coords.x = g_value_get_double(value);
		break;
	case PROP_Y:
		impl->coords.y = g_value_get_double(value);
		break;
	case PROP_COLOR:
		impl->color = g_value_get_uint(value);
		break;
	case PROP_FONT_SIZE:
		impl->font_size    = g_value_get_double(value);
		impl->needs_layout = TRUE;
		break;
	case PROP_TEXT:
		free(impl->text);
		impl->text         = g_value_dup_string(value);
		impl->needs_layout = TRUE;
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		return;
	}
	if (impl->needs_layout) {
		if (GANV_IS_NODE(GANV_ITEM(text)->impl->parent)) {
			GANV_NODE(GANV_ITEM(text)->impl->parent)->impl->must_resize = TRUE;
		}
	}
	ganv_item_request_update(GANV_ITEM(text));
}

static void
ganv_text_get_property(GObject*    object,
                       guint       prop_id,
                       GValue*     value,
                       GParamSpec* pspec)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(GANV_IS_TEXT(object));

	GanvText*        text = GANV_TEXT(object);
	GanvTextPrivate* impl = text->impl;

	if (impl->needs_layout && (prop_id == PROP_WIDTH
	                           || prop_id == PROP_HEIGHT)) {
		ganv_text_layout(text);
	}

	switch (prop_id) {
		GET_CASE(TEXT, string, impl->text)
		GET_CASE(X, double, impl->coords.x)
		GET_CASE(Y, double, impl->coords.y)
		GET_CASE(WIDTH, double, impl->coords.width)
		GET_CASE(HEIGHT, double, impl->coords.height)
		GET_CASE(COLOR, uint, impl->color)
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
	GanvText*        text = GANV_TEXT(item);
	GanvTextPrivate* impl = text->impl;

	if (impl->needs_layout) {
		ganv_text_layout(text);
	}

	*x1 = impl->coords.x;
	*y1 = impl->coords.y;
	*x2 = impl->coords.x + impl->coords.width;
	*y2 = impl->coords.y + impl->coords.height;
}

static void
ganv_text_bounds(GanvItem* item,
                 double* x1, double* y1,
                 double* x2, double* y2)
{
	ganv_text_bounds_item(item, x1, y1, x2, y2);
}

static void
ganv_text_update(GanvItem* item, int flags)
{
	// Update world-relative bounding box
	ganv_text_bounds(item, &item->impl->x1, &item->impl->y1, &item->impl->x2, &item->impl->y2);
	ganv_item_i2w_pair(item, &item->impl->x1, &item->impl->y1, &item->impl->x2, &item->impl->y2);

	ganv_canvas_request_redraw_w(
		item->impl->canvas, item->impl->x1, item->impl->y1, item->impl->x2, item->impl->y2);

	parent_class->update(item, flags);
}

static double
ganv_text_point(GanvItem* item, double x, double y, GanvItem** actual_item)
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
               cairo_t* cr, double cx, double cy, double cw, double ch)
{
	GanvText*        text = GANV_TEXT(item);
	GanvTextPrivate* impl = text->impl;

	double wx = impl->coords.x;
	double wy = impl->coords.y;
	ganv_item_i2w(item, &wx, &wy);

	if (impl->needs_layout) {
		ganv_text_layout(text);
	}

	double r, g, b, a;
	color_to_rgba(impl->color, &r, &g, &b, &a);

	cairo_set_source_rgba(cr, r, g, b, a);
	cairo_move_to(cr, wx, wy);
	pango_cairo_show_layout(cr, impl->layout);
}

static void
ganv_text_class_init(GanvTextClass* klass)
{
	GObjectClass*   gobject_class = (GObjectClass*)klass;
	GtkObjectClass* object_class  = (GtkObjectClass*)klass;
	GanvItemClass*  item_class    = (GanvItemClass*)klass;

	parent_class = GANV_ITEM_CLASS(g_type_class_peek_parent(klass));

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
			G_PARAM_READABLE));

	g_object_class_install_property(
		gobject_class, PROP_HEIGHT, g_param_spec_double(
			"height",
			_("Height"),
			_("The current height of the text."),
			-G_MAXDOUBLE, G_MAXDOUBLE,
			1.0,
			G_PARAM_READABLE));

	g_object_class_install_property(
		gobject_class, PROP_COLOR, g_param_spec_uint(
			"color",
			_("Color"),
			_("The color of the text."),
			0, G_MAXUINT,
			DEFAULT_TEXT_COLOR,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_FONT_SIZE, g_param_spec_double(
			"font-size",
			_("Font size"),
			_("The font size in points."),
			-G_MAXDOUBLE, G_MAXDOUBLE,
			0.0,
			G_PARAM_READWRITE));


	object_class->destroy = ganv_text_destroy;

	item_class->update = ganv_text_update;
	item_class->bounds = ganv_text_bounds;
	item_class->point  = ganv_text_point;
	item_class->draw   = ganv_text_draw;
}
