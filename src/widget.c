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

/* Based on GnomeCanvasWidget, by Federico Mena <federico@nuclecu.unam.mx>
 * Copyright 1997-2000 Free Software Foundation
 */

#include <math.h>

#include <gtk/gtksignal.h>

#include "ganv/canvas.h"
#include "ganv/widget.h"

#include "./gettext.h"
#include "./ganv-private.h"

G_DEFINE_TYPE_WITH_CODE(GanvWidget, ganv_widget, GANV_TYPE_ITEM,
                        G_ADD_PRIVATE(GanvWidget))

static GanvItemClass* parent_class;

enum {
	PROP_0,
	PROP_WIDGET,
	PROP_X,
	PROP_Y,
	PROP_WIDTH,
	PROP_HEIGHT,
	PROP_ANCHOR,
	PROP_SIZE_PIXELS
};

static void
ganv_widget_init(GanvWidget* witem)
{
	GanvWidgetPrivate* impl =
	    (GanvWidgetPrivate*)ganv_widget_get_instance_private(witem);

	witem->impl              = impl;
	witem->impl->x           = 0.0;
	witem->impl->y           = 0.0;
	witem->impl->width       = 0.0;
	witem->impl->height      = 0.0;
	witem->impl->anchor      = GTK_ANCHOR_NW;
	witem->impl->size_pixels = FALSE;
}

static void
ganv_widget_destroy(GtkObject* object)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(GANV_IS_WIDGET(object));

	GanvWidget* witem = GANV_WIDGET(object);

	if (witem->impl->widget && !witem->impl->in_destroy) {
		g_signal_handler_disconnect(witem->impl->widget, witem->impl->destroy_id);
		gtk_widget_destroy(witem->impl->widget);
		witem->impl->widget = NULL;
	}

	if (GTK_OBJECT_CLASS(parent_class)->destroy) {
		(*GTK_OBJECT_CLASS(parent_class)->destroy)(object);
	}
}

static void
recalc_bounds(GanvWidget* witem)
{
	GanvItem* item = GANV_ITEM(witem);

	/* Get world coordinates */

	double wx = witem->impl->x;
	double wy = witem->impl->y;
	ganv_item_i2w(item, &wx, &wy);

	/* Get canvas pixel coordinates */

	ganv_canvas_w2c(item->impl->canvas, wx, wy, &witem->impl->cx, &witem->impl->cy);

	/* Anchor widget item */

	switch (witem->impl->anchor) {
	case GTK_ANCHOR_N:
	case GTK_ANCHOR_CENTER:
	case GTK_ANCHOR_S:
		witem->impl->cx -= witem->impl->cwidth / 2;
		break;

	case GTK_ANCHOR_NE:
	case GTK_ANCHOR_E:
	case GTK_ANCHOR_SE:
		witem->impl->cx -= witem->impl->cwidth;
		break;

	default:
		break;
	}

	switch (witem->impl->anchor) {
	case GTK_ANCHOR_W:
	case GTK_ANCHOR_CENTER:
	case GTK_ANCHOR_E:
		witem->impl->cy -= witem->impl->cheight / 2;
		break;

	case GTK_ANCHOR_SW:
	case GTK_ANCHOR_S:
	case GTK_ANCHOR_SE:
		witem->impl->cy -= witem->impl->cheight;
		break;

	default:
		break;
	}

	/* Bounds */

	item->impl->x1 = witem->impl->cx;
	item->impl->y1 = witem->impl->cy;
	item->impl->x2 = witem->impl->cx + witem->impl->cwidth;
	item->impl->y2 = witem->impl->cy + witem->impl->cheight;

	int zoom_xofs, zoom_yofs;
	ganv_canvas_get_zoom_offsets(item->impl->canvas, &zoom_xofs, &zoom_yofs);
	if (witem->impl->widget) {
		gtk_layout_move(GTK_LAYOUT(item->impl->canvas), witem->impl->widget,
		                witem->impl->cx + zoom_xofs,
		                witem->impl->cy + zoom_yofs);
	}
}

static void
do_destroy(GtkObject* object, gpointer data)
{
	(void)object;

	GanvWidget* witem = GANV_WIDGET(data);

	witem->impl->in_destroy = TRUE;
	gtk_object_destroy(GTK_OBJECT(data));
}

static void
ganv_widget_set_property(GObject*      object,
                         guint         param_id,
                         const GValue* value,
                         GParamSpec*   pspec)
{
	GanvItem*   item        = GANV_ITEM(object);
	GanvWidget* witem       = GANV_WIDGET(object);
	int         update      = FALSE;
	int         calc_bounds = FALSE;
	GObject*    obj;

	switch (param_id) {
	case PROP_WIDGET:
		if (witem->impl->widget) {
			g_signal_handler_disconnect(witem->impl->widget, witem->impl->destroy_id);
			gtk_container_remove(GTK_CONTAINER(item->impl->canvas), witem->impl->widget);
		}

		obj = (GObject*)g_value_get_object(value);
		if (obj) {
			witem->impl->widget     = GTK_WIDGET(obj);
			witem->impl->destroy_id = g_signal_connect(obj, "destroy",
			                                     G_CALLBACK(do_destroy),
			                                     witem);
			int zoom_xofs, zoom_yofs;
			ganv_canvas_get_zoom_offsets(item->impl->canvas, &zoom_xofs, &zoom_yofs);

			gtk_layout_put(GTK_LAYOUT(item->impl->canvas), witem->impl->widget,
			               witem->impl->cx + zoom_xofs,
			               witem->impl->cy + zoom_yofs);
		}

		update = TRUE;
		break;

	case PROP_X:
		if (witem->impl->x != g_value_get_double(value)) {
			witem->impl->x    = g_value_get_double(value);
			calc_bounds = TRUE;
		}
		break;

	case PROP_Y:
		if (witem->impl->y != g_value_get_double(value)) {
			witem->impl->y    = g_value_get_double(value);
			calc_bounds = TRUE;
		}
		break;

	case PROP_WIDTH:
		if (witem->impl->width != fabs(g_value_get_double(value))) {
			witem->impl->width = fabs(g_value_get_double(value));
			update       = TRUE;
		}
		break;

	case PROP_HEIGHT:
		if (witem->impl->height != fabs(g_value_get_double(value))) {
			witem->impl->height = fabs(g_value_get_double(value));
			update        = TRUE;
		}
		break;

	case PROP_ANCHOR:
		if (witem->impl->anchor != (GtkAnchorType)g_value_get_enum(value)) {
			witem->impl->anchor = (GtkAnchorType)g_value_get_enum(value);
			update        = TRUE;
		}
		break;

	case PROP_SIZE_PIXELS:
		if (witem->impl->size_pixels != g_value_get_boolean(value)) {
			witem->impl->size_pixels = g_value_get_boolean(value);
			update             = TRUE;
		}
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, param_id, pspec);
		break;
	}

	if (update) {
		(*GANV_ITEM_GET_CLASS(item)->update)(item, 0);
	}

	if (calc_bounds) {
		recalc_bounds(witem);
	}
}

static void
ganv_widget_get_property(GObject*    object,
                         guint       param_id,
                         GValue*     value,
                         GParamSpec* pspec)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(GANV_IS_WIDGET(object));

	GanvWidget* witem = GANV_WIDGET(object);

	switch (param_id) {
	case PROP_WIDGET:
		g_value_set_object(value, (GObject*)witem->impl->widget);
		break;

	case PROP_X:
		g_value_set_double(value, witem->impl->x);
		break;

	case PROP_Y:
		g_value_set_double(value, witem->impl->y);
		break;

	case PROP_WIDTH:
		g_value_set_double(value, witem->impl->width);
		break;

	case PROP_HEIGHT:
		g_value_set_double(value, witem->impl->height);
		break;

	case PROP_ANCHOR:
		g_value_set_enum(value, witem->impl->anchor);
		break;

	case PROP_SIZE_PIXELS:
		g_value_set_boolean(value, witem->impl->size_pixels);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, param_id, pspec);
		break;
	}
}

static void
ganv_widget_update(GanvItem* item, int flags)
{
	GanvWidget* witem = GANV_WIDGET(item);

	if (parent_class->update) {
		(*parent_class->update)(item, flags);
	}

	if (witem->impl->widget) {
		const double pixels_per_unit = ganv_canvas_get_zoom(item->impl->canvas);
		if (witem->impl->size_pixels) {
			witem->impl->cwidth  = (int)(witem->impl->width + 0.5);
			witem->impl->cheight = (int)(witem->impl->height + 0.5);
		} else {
			witem->impl->cwidth  = (int)(witem->impl->width * pixels_per_unit + 0.5);
			witem->impl->cheight = (int)(witem->impl->height * pixels_per_unit + 0.5);
		}

		gtk_widget_set_size_request(witem->impl->widget, witem->impl->cwidth, witem->impl->cheight);
	} else {
		witem->impl->cwidth  = 0.0;
		witem->impl->cheight = 0.0;
	}

	recalc_bounds(witem);
}

static void
ganv_widget_draw(GanvItem* item,
                 cairo_t* cr, double cx, double cy, double cw, double ch)
{
	(void)cr;
	(void)cx;
	(void)cy;
	(void)cw;
	(void)ch;

	GanvWidget* witem = GANV_WIDGET(item);

	if (witem->impl->widget) {
		gtk_widget_queue_draw(witem->impl->widget);
	}
}

static double
ganv_widget_point(GanvItem* item, double x, double y, GanvItem** actual_item)
{
	GanvWidget* witem = GANV_WIDGET(item);

	*actual_item = item;

	double x1, y1;
	ganv_canvas_c2w(item->impl->canvas, witem->impl->cx, witem->impl->cy, &x1, &y1);

	const double pixels_per_unit = ganv_canvas_get_zoom(item->impl->canvas);

	double x2 = x1 + (witem->impl->cwidth - 1) / pixels_per_unit;
	double y2 = y1 + (witem->impl->cheight - 1) / pixels_per_unit;

	/* Is point inside widget bounds? */

	if ((x >= x1) && (y >= y1) && (x <= x2) && (y <= y2)) {
		return 0.0;
	}

	/* Point is outside widget bounds */

	double dx;
	if (x < x1) {
		dx = x1 - x;
	} else if (x > x2) {
		dx = x - x2;
	} else {
		dx = 0.0;
	}

	double dy;
	if (y < y1) {
		dy = y1 - y;
	} else if (y > y2) {
		dy = y - y2;
	} else {
		dy = 0.0;
	}

	return sqrt(dx * dx + dy * dy);
}

static void
ganv_widget_bounds(GanvItem* item, double* x1, double* y1, double* x2, double* y2)
{
	GanvWidget* witem = GANV_WIDGET(item);

	*x1 = witem->impl->x;
	*y1 = witem->impl->y;

	switch (witem->impl->anchor) {
	case GTK_ANCHOR_NW:
	case GTK_ANCHOR_W:
	case GTK_ANCHOR_SW:
		break;

	case GTK_ANCHOR_N:
	case GTK_ANCHOR_CENTER:
	case GTK_ANCHOR_S:
		*x1 -= witem->impl->width / 2.0;
		break;

	case GTK_ANCHOR_NE:
	case GTK_ANCHOR_E:
	case GTK_ANCHOR_SE:
		*x1 -= witem->impl->width;
		break;

	default:
		break;
	}

	switch (witem->impl->anchor) {
	case GTK_ANCHOR_NW:
	case GTK_ANCHOR_N:
	case GTK_ANCHOR_NE:
		break;

	case GTK_ANCHOR_W:
	case GTK_ANCHOR_CENTER:
	case GTK_ANCHOR_E:
		*y1 -= witem->impl->height / 2.0;
		break;

	case GTK_ANCHOR_SW:
	case GTK_ANCHOR_S:
	case GTK_ANCHOR_SE:
		*y1 -= witem->impl->height;
		break;

	default:
		break;
	}

	*x2 = *x1 + witem->impl->width;
	*y2 = *y1 + witem->impl->height;
}

static void
ganv_widget_class_init(GanvWidgetClass* klass)
{
	GObjectClass*   gobject_class = (GObjectClass*)klass;
	GtkObjectClass* object_class  = (GtkObjectClass*)klass;
	GanvItemClass*  item_class    = (GanvItemClass*)klass;

	parent_class = (GanvItemClass*)g_type_class_peek_parent(klass);

	gobject_class->set_property = ganv_widget_set_property;
	gobject_class->get_property = ganv_widget_get_property;

	g_object_class_install_property(
		gobject_class, PROP_WIDGET, g_param_spec_object(
			"widget", _("Widget"),
			_("The widget to embed in this item."),
			GTK_TYPE_WIDGET,
			(GParamFlags)(G_PARAM_READABLE | G_PARAM_WRITABLE)));

	g_object_class_install_property(
		gobject_class, PROP_X, g_param_spec_double(
			"x", _("x"),
			_("The x coordinate of the anchor"),
			-G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_Y, g_param_spec_double(
			"y", _("y"),
			_("The x coordinate of the anchor"),
			-G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_WIDTH, g_param_spec_double(
			"width", _("Width"),
			_("The width of the widget."),
			-G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_HEIGHT, g_param_spec_double(
			"height", _("Height"),
			_("The height of the widget."),
			-G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_ANCHOR, g_param_spec_enum(
			"anchor", _("Anchor"),
			_("The anchor point of the widget."),
			GTK_TYPE_ANCHOR_TYPE,
			GTK_ANCHOR_NW,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_SIZE_PIXELS, g_param_spec_boolean(
			"size-pixels", ("Size is in pixels"),
			_("Specifies whether the widget size is specified in pixels or"
			  " canvas units. If it is in pixels, then the widget will not"
			  " be scaled when the canvas zoom factor changes.  Otherwise,"
			  " it will be scaled."),
			FALSE,
			G_PARAM_READWRITE));

	object_class->destroy = ganv_widget_destroy;

	item_class->update = ganv_widget_update;
	item_class->point  = ganv_widget_point;
	item_class->bounds = ganv_widget_bounds;
	item_class->draw   = ganv_widget_draw;
}
