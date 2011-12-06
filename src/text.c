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

#include <stdint.h>
#include <string.h>
#include <math.h>

#include <gtk/gtkstyle.h>
#include <libgnomecanvas/libgnomecanvas.h>

#include "ganv/canvas.h"
#include "ganv/text.h"

#include "./color.h"
#include "./boilerplate.h"
#include "./gettext.h"

G_DEFINE_TYPE(GanvText, ganv_text, GNOME_TYPE_CANVAS_ITEM)

static GnomeCanvasItemClass* parent_class;

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
	memset(&text->coords, '\0', sizeof(GanvTextCoords));
	text->coords.width  = 1.0;
	text->coords.height = 1.0;
	text->old_coords = text->coords;

	text->surface = NULL;
	text->text    = NULL;
	text->color   = 0xFFFFFFFF;
}

static void
ganv_text_destroy(GtkObject* object)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(GANV_IS_TEXT(object));

	GanvText* text = GANV_TEXT(object);

	if (text->text) {
		g_free(text->text);
		text->text = NULL;
	}

	if (text->surface) {
		cairo_surface_destroy(text->surface);
		text->surface = NULL;
	}

	if (GTK_OBJECT_CLASS(parent_class)->destroy) {
		(*GTK_OBJECT_CLASS(parent_class)->destroy)(object);
	}
}

static void
ganv_text_layout(GanvText* text)
{
	GnomeCanvasItem*  item      = GNOME_CANVAS_ITEM(text);
	GanvCanvas* canvas    = GANV_CANVAS(item->canvas);
	GtkWidget*        widget    = GTK_WIDGET(canvas);
	double            font_size = ganv_canvas_get_font_size(canvas);
	guint             color     = 0xFFFFFFFF;

	GtkStyle*             style   = gtk_rc_get_style(widget);
	PangoFontDescription* font    = pango_font_description_copy(style->font_desc);
	PangoLayout*          layout  = gtk_widget_create_pango_layout(widget, text->text);
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

	text->coords.width = width;
	text->coords.height = height;

	if (text->surface) {
		cairo_surface_destroy(text->surface);
	}

	text->surface = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, width, height);

	cairo_t* cr = cairo_create(text->surface);

	double r, g, b, a;
	color_to_rgba(color, &r, &g, &b, &a);

	cairo_set_source_rgba(cr, r, g, b, a);
	cairo_move_to(cr, 0, 0);
	pango_cairo_show_layout(cr, layout);

	cairo_destroy(cr);
	g_object_unref(layout);
	pango_font_description_free(font);
	
	gnome_canvas_item_request_update(GNOME_CANVAS_ITEM(text));
}

static void
ganv_text_set_property(GObject*      object,
                       guint         prop_id,
                       const GValue* value,
                       GParamSpec*   pspec)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(GANV_IS_TEXT(object));

	GanvText* text = GANV_TEXT(object);

	switch (prop_id) {
		SET_CASE(X, double, text->coords.x);
		SET_CASE(Y, double, text->coords.y);
		SET_CASE(WIDTH, double, text->coords.width);
		SET_CASE(HEIGHT, double, text->coords.height);
		SET_CASE(COLOR, uint, text->color)
	case PROP_TEXT:
		free(text->text);
		text->text = g_value_dup_string(value);
		ganv_text_layout(text);
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

	GanvText* text = GANV_TEXT(object);

	switch (prop_id) {
		GET_CASE(TEXT, string, text->text)
			GET_CASE(X, double, text->coords.x);
		GET_CASE(Y, double, text->coords.y);
		GET_CASE(WIDTH, double, text->coords.width);
		GET_CASE(HEIGHT, double, text->coords.height);
		GET_CASE(COLOR, uint, text->color)
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
ganv_text_bounds_item(GnomeCanvasItem* item,
                      double* x1, double* y1,
                      double* x2, double* y2)
{
	GanvText* text = GANV_TEXT(item);

	*x1 = MIN(text->coords.x, text->coords.x + text->coords.width);
	*y1 = MIN(text->coords.y, text->coords.y + text->coords.height);
	*x2 = MAX(text->coords.x, text->coords.x + text->coords.width);
	*y2 = MAX(text->coords.y, text->coords.y + text->coords.height);
}

static void
ganv_text_bounds(GnomeCanvasItem* item,
                 double* x1, double* y1,
                 double* x2, double* y2)
{
	ganv_text_bounds_item(item, x1, y1, x2, y2);
	gnome_canvas_item_i2w(item->parent, x1, y1);
	gnome_canvas_item_i2w(item->parent, x2, y2);
}

static void
ganv_text_update(GnomeCanvasItem* item,
                 double*          affine,
                 ArtSVP*          clip_path,
                 int              flags)
{
	double x1, y1, x2, y2;
	ganv_text_bounds(item, &x1, &y1, &x2, &y2);
	gnome_canvas_request_redraw(item->canvas, x1, y1, x2, y2);

	// I have no idea why this is necessary
	item->x1 = x1;
	item->y1 = y1;
	item->x2 = x2;
	item->y2 = y2;

	parent_class->update(item, affine, clip_path, flags);
}

static double
ganv_text_point(GnomeCanvasItem*  item,
                double x, double y,
                int cx, int cy,
                GnomeCanvasItem** actual_item)
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
ganv_text_render(GnomeCanvasItem* item,
                 GnomeCanvasBuf*  buf)
{
	// Not implemented
}

static void
ganv_text_draw(GnomeCanvasItem* item,
               GdkDrawable* drawable,
               int x, int y,
               int width, int height)
{
	GanvText* text = GANV_TEXT(item);
	cairo_t*        cr   = gdk_cairo_create(drawable);

	double wx = text->coords.x;
	double wy = text->coords.y;
	gnome_canvas_item_i2w(item, &wx, &wy);

	// Round to the nearest pixel so text isn't blurry
	wx = lrint(wx - x);
	wy = lrint(wy - y);
	
	cairo_set_source_surface(cr, text->surface, wx, wy);
	cairo_paint(cr);

	cairo_destroy(cr);
}

static void
ganv_text_class_init(GanvTextClass* class)
{
	GObjectClass*         gobject_class = (GObjectClass*)class;
	GtkObjectClass*       object_class  = (GtkObjectClass*)class;
	GnomeCanvasItemClass* item_class    = (GnomeCanvasItemClass*)class;

	parent_class = GNOME_CANVAS_ITEM_CLASS(g_type_class_peek_parent(class));

	gobject_class->set_property = ganv_text_set_property;
	gobject_class->get_property = ganv_text_get_property;

	g_object_class_install_property(
		gobject_class, PROP_TEXT, g_param_spec_string(
			"text",
			_("text"),
			_("the text to display"),
			NULL,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_X, g_param_spec_double(
			"x",
			_("x"),
			_("x coordinate"),
			-G_MAXDOUBLE, G_MAXDOUBLE,
			0.0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_Y, g_param_spec_double(
			"y",
			_("y"),
			_("y coordinate"),
			-G_MAXDOUBLE, G_MAXDOUBLE,
			0.0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_WIDTH, g_param_spec_double(
			"width",
			_("width"),
			_("width"),
			-G_MAXDOUBLE, G_MAXDOUBLE,
			1.0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_HEIGHT, g_param_spec_double(
			"height",
			_("height"),
			_("height"),
			-G_MAXDOUBLE, G_MAXDOUBLE,
			1.0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_COLOR, g_param_spec_uint(
			"color",
			_("color"),
			_("the color of the text"),
			0, G_MAXUINT,
			DEFAULT_TEXT_COLOR,
			G_PARAM_READWRITE));

	object_class->destroy = ganv_text_destroy;

	item_class->update = ganv_text_update;
	item_class->bounds = ganv_text_bounds;
	item_class->point  = ganv_text_point;
	item_class->render = ganv_text_render;
	item_class->draw   = ganv_text_draw;
}
