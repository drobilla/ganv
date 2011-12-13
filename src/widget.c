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

/* Based on GnomeCanvasWidget, by Federico Mena <federico@nuclecu.unam.mx>
 * Copyright 1997-2000 Free Software Foundation
 */

#include <math.h>

#include <gtk/gtksignal.h>

#include "ganv/widget.h"

#include "./gettext.h"

G_DEFINE_TYPE(GanvWidget, ganv_widget, GANV_TYPE_ITEM)

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
	witem->x           = 0.0;
	witem->y           = 0.0;
	witem->width       = 0.0;
	witem->height      = 0.0;
	witem->anchor      = GTK_ANCHOR_NW;
	witem->size_pixels = FALSE;
}

static void
ganv_widget_destroy(GtkObject* object)
{
	GanvWidget* witem;

	g_return_if_fail(object != NULL);
	g_return_if_fail(GANV_IS_WIDGET(object));

	witem = GANV_WIDGET(object);

	if (witem->widget && !witem->in_destroy) {
		g_signal_handler_disconnect(witem->widget, witem->destroy_id);
		gtk_widget_destroy(witem->widget);
		witem->widget = NULL;
	}

	if (GTK_OBJECT_CLASS(parent_class)->destroy) {
		(*GTK_OBJECT_CLASS(parent_class)->destroy)(object);
	}
}

static void
recalc_bounds(GanvWidget* witem)
{
	GanvItem* item;
	double    wx, wy;

	item = GANV_ITEM(witem);

	/* Get world coordinates */

	wx = witem->x;
	wy = witem->y;
	ganv_item_i2w(item, &wx, &wy);

	/* Get canvas pixel coordinates */

	ganv_canvas_base_w2c(item->canvas, wx, wy, &witem->cx, &witem->cy);

	/* Anchor widget item */

	switch (witem->anchor) {
	case GTK_ANCHOR_N:
	case GTK_ANCHOR_CENTER:
	case GTK_ANCHOR_S:
		witem->cx -= witem->cwidth / 2;
		break;

	case GTK_ANCHOR_NE:
	case GTK_ANCHOR_E:
	case GTK_ANCHOR_SE:
		witem->cx -= witem->cwidth;
		break;

	default:
		break;
	}

	switch (witem->anchor) {
	case GTK_ANCHOR_W:
	case GTK_ANCHOR_CENTER:
	case GTK_ANCHOR_E:
		witem->cy -= witem->cheight / 2;
		break;

	case GTK_ANCHOR_SW:
	case GTK_ANCHOR_S:
	case GTK_ANCHOR_SE:
		witem->cy -= witem->cheight;
		break;

	default:
		break;
	}

	/* Bounds */

	item->x1 = witem->cx;
	item->y1 = witem->cy;
	item->x2 = witem->cx + witem->cwidth;
	item->y2 = witem->cy + witem->cheight;

	if (witem->widget) {
		gtk_layout_move(GTK_LAYOUT(item->canvas), witem->widget,
		                witem->cx + item->canvas->zoom_xofs,
		                witem->cy + item->canvas->zoom_yofs);
	}
}

static void
do_destroy(GtkObject* object, gpointer data)
{
	GanvWidget* witem = GANV_WIDGET(data);

	witem->in_destroy = TRUE;
	gtk_object_destroy(data);
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
		if (witem->widget) {
			g_signal_handler_disconnect(witem->widget, witem->destroy_id);
			gtk_container_remove(GTK_CONTAINER(item->canvas), witem->widget);
		}

		obj = g_value_get_object(value);
		if (obj) {
			witem->widget     = GTK_WIDGET(obj);
			witem->destroy_id = g_signal_connect(obj, "destroy",
			                                     G_CALLBACK(do_destroy),
			                                     witem);
			gtk_layout_put(GTK_LAYOUT(item->canvas), witem->widget,
			               witem->cx + item->canvas->zoom_xofs,
			               witem->cy + item->canvas->zoom_yofs);
		}

		update = TRUE;
		break;

	case PROP_X:
		if (witem->x != g_value_get_double(value)) {
			witem->x    = g_value_get_double(value);
			calc_bounds = TRUE;
		}
		break;

	case PROP_Y:
		if (witem->y != g_value_get_double(value)) {
			witem->y    = g_value_get_double(value);
			calc_bounds = TRUE;
		}
		break;

	case PROP_WIDTH:
		if (witem->width != fabs(g_value_get_double(value))) {
			witem->width = fabs(g_value_get_double(value));
			update       = TRUE;
		}
		break;

	case PROP_HEIGHT:
		if (witem->height != fabs(g_value_get_double(value))) {
			witem->height = fabs(g_value_get_double(value));
			update        = TRUE;
		}
		break;

	case PROP_ANCHOR:
		if (witem->anchor != (GtkAnchorType)g_value_get_enum(value)) {
			witem->anchor = g_value_get_enum(value);
			update        = TRUE;
		}
		break;

	case PROP_SIZE_PIXELS:
		if (witem->size_pixels != g_value_get_boolean(value)) {
			witem->size_pixels = g_value_get_boolean(value);
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
	GanvWidget* witem;

	g_return_if_fail(object != NULL);
	g_return_if_fail(GANV_IS_WIDGET(object));

	witem = GANV_WIDGET(object);

	switch (param_id) {
	case PROP_WIDGET:
		g_value_set_object(value, (GObject*)witem->widget);
		break;

	case PROP_X:
		g_value_set_double(value, witem->x);
		break;

	case PROP_Y:
		g_value_set_double(value, witem->y);
		break;

	case PROP_WIDTH:
		g_value_set_double(value, witem->width);
		break;

	case PROP_HEIGHT:
		g_value_set_double(value, witem->height);
		break;

	case PROP_ANCHOR:
		g_value_set_enum(value, witem->anchor);
		break;

	case PROP_SIZE_PIXELS:
		g_value_set_boolean(value, witem->size_pixels);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, param_id, pspec);
		break;
	}
}

static void
ganv_widget_update(GanvItem* item, int flags)
{
	GanvWidget* witem;

	witem = GANV_WIDGET(item);

	if (parent_class->update) {
		(*parent_class->update)(item, flags);
	}

	if (witem->widget) {
		if (witem->size_pixels) {
			witem->cwidth  = (int)(witem->width + 0.5);
			witem->cheight = (int)(witem->height + 0.5);
		} else {
			witem->cwidth  = (int)(witem->width * item->canvas->pixels_per_unit + 0.5);
			witem->cheight = (int)(witem->height * item->canvas->pixels_per_unit + 0.5);
		}

		gtk_widget_set_size_request(witem->widget, witem->cwidth, witem->cheight);
	} else {
		witem->cwidth  = 0.0;
		witem->cheight = 0.0;
	}

	recalc_bounds(witem);
}

static void
ganv_widget_draw(GanvItem* item,
                 cairo_t* cr,
                 int x, int y,
                 int width, int height)
{
#if 0
	GanvWidget* witem;

	witem = GANV_WIDGET(item);

	if (witem->widget) {
		gtk_widget_queue_draw(witem->widget);
	}
#endif
}

static double
ganv_widget_point(GanvItem* item, double x, double y,
                  int cx, int cy, GanvItem** actual_item)
{
	GanvWidget* witem;
	double             x1, y1, x2, y2;
	double             dx, dy;

	witem = GANV_WIDGET(item);

	*actual_item = item;

	ganv_canvas_base_c2w(item->canvas, witem->cx, witem->cy, &x1, &y1);

	x2 = x1 + (witem->cwidth - 1) / item->canvas->pixels_per_unit;
	y2 = y1 + (witem->cheight - 1) / item->canvas->pixels_per_unit;

	/* Is point inside widget bounds? */

	if ((x >= x1) && (y >= y1) && (x <= x2) && (y <= y2)) {
		return 0.0;
	}

	/* Point is outside widget bounds */

	if (x < x1) {
		dx = x1 - x;
	} else if (x > x2) {
		dx = x - x2;
	} else {
		dx = 0.0;
	}

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
	GanvWidget* witem;

	witem = GANV_WIDGET(item);

	*x1 = witem->x;
	*y1 = witem->y;

	switch (witem->anchor) {
	case GTK_ANCHOR_NW:
	case GTK_ANCHOR_W:
	case GTK_ANCHOR_SW:
		break;

	case GTK_ANCHOR_N:
	case GTK_ANCHOR_CENTER:
	case GTK_ANCHOR_S:
		*x1 -= witem->width / 2.0;
		break;

	case GTK_ANCHOR_NE:
	case GTK_ANCHOR_E:
	case GTK_ANCHOR_SE:
		*x1 -= witem->width;
		break;

	default:
		break;
	}

	switch (witem->anchor) {
	case GTK_ANCHOR_NW:
	case GTK_ANCHOR_N:
	case GTK_ANCHOR_NE:
		break;

	case GTK_ANCHOR_W:
	case GTK_ANCHOR_CENTER:
	case GTK_ANCHOR_E:
		*y1 -= witem->height / 2.0;
		break;

	case GTK_ANCHOR_SW:
	case GTK_ANCHOR_S:
	case GTK_ANCHOR_SE:
		*y1 -= witem->height;
		break;

	default:
		break;
	}

	*x2 = *x1 + witem->width;
	*y2 = *y1 + witem->height;
}

static void
ganv_widget_class_init(GanvWidgetClass* class)
{
	GObjectClass*   gobject_class = (GObjectClass*)class;
	GtkObjectClass* object_class  = (GtkObjectClass*)class;
	GanvItemClass*  item_class    = (GanvItemClass*)class;

	parent_class = g_type_class_peek_parent(class);

	gobject_class->set_property = ganv_widget_set_property;
	gobject_class->get_property = ganv_widget_get_property;

	g_object_class_install_property(
		gobject_class, PROP_WIDGET, g_param_spec_object(
			"widget", _("Widget"),
			_("The widget to embed in this item."),
			GTK_TYPE_WIDGET,
			(G_PARAM_READABLE | G_PARAM_WRITABLE)));

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
