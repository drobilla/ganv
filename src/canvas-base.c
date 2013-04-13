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

/* Based on GnomeCanvas, by Federico Mena <federico@nuclecu.unam.mx>
 * and Raph Levien <raph@gimp.org>
 * Copyright 1997-2000 Free Software Foundation
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <gtk/gtk.h>
#include <cairo.h>

#include "ganv/canvas-base.h"
#include "ganv/group.h"

#include "./ganv-marshal.h"
#include "./ganv-private.h"
#include "./gettext.h"

/* We must run our idle update handler *before* GDK wants to redraw. */
#define CANVAS_IDLE_PRIORITY (GDK_PRIORITY_REDRAW - 5)

static void ganv_canvas_base_request_update(GanvCanvasBase* canvas);
static void add_idle(GanvCanvasBase* canvas);

typedef struct {
	int x;
	int y;
	int width;
	int height;
} IRect;

/* Some convenience stuff */
#define GCI_UPDATE_MASK (GANV_CANVAS_BASE_UPDATE_REQUESTED | GANV_CANVAS_BASE_UPDATE_AFFINE \
                         | GANV_CANVAS_BASE_UPDATE_VISIBILITY)

enum {
	ITEM_PROP_0,
	ITEM_PROP_PARENT,
	ITEM_PROP_X,
	ITEM_PROP_Y,
	ITEM_PROP_MANAGED
};

enum {
	ITEM_EVENT,
	ITEM_LAST_SIGNAL
};

static int emit_event(GanvCanvasBase* canvas, GdkEvent* event);

static guint item_signals[ITEM_LAST_SIGNAL];

G_DEFINE_TYPE(GanvItem, ganv_item, GTK_TYPE_OBJECT)

static GtkObjectClass * item_parent_class;

/* Object initialization function for GanvItem */
static void
ganv_item_init(GanvItem* item)
{
	item->object.flags |= GANV_ITEM_VISIBLE;
	item->managed       = FALSE;
}

/**
 * ganv_item_new:
 * @parent: The parent group for the new item.
 * @type: The object type of the item.
 * @first_arg_name: A list of object argument name/value pairs, NULL-terminated,
 * used to configure the item.  For example, "fill_color", "black",
 * "width_units", 5.0, NULL.
 * @Varargs:
 *
 * Creates a new canvas item with @parent as its parent group.  The item is
 * created at the top of its parent's stack, and starts up as visible.  The item
 * is of the specified @type, for example, it can be
 * ganv_canvas_base_rect_get_type().  The list of object arguments/value pairs is
 * used to configure the item. If you need to pass construct time parameters, you
 * should use g_object_new() to pass the parameters and
 * ganv_item_construct() to set up the canvas item.
 *
 * Return value: (transfer full): The newly-created item.
 **/
GanvItem*
ganv_item_new(GanvItem* parent, GType type, const gchar* first_arg_name, ...)
{
	GanvItem* item;
	va_list   args;

	g_return_val_if_fail(g_type_is_a(type, ganv_item_get_type()), NULL);

	item = GANV_ITEM(g_object_new(type, NULL));

	va_start(args, first_arg_name);
	ganv_item_construct(item, parent, first_arg_name, args);
	va_end(args);

	return item;
}

/* Performs post-creation operations on a canvas item (adding it to its parent
 * group, etc.)
 */
static void
item_post_create_setup(GanvItem* item)
{
	GanvItemClass* parent_class = GANV_ITEM_GET_CLASS(item->parent);
	if (!item->managed) {
		if (parent_class->add) {
			parent_class->add(item->parent, item);
		} else {
			g_warning("item added to non-parent item\n");
		}
	}
	ganv_canvas_base_request_redraw(item->canvas,
	                                item->x1, item->y1,
	                                item->x2 + 1, item->y2 + 1);
	item->canvas->need_repick = TRUE;
}

/* Set_property handler for canvas items */
static void
ganv_item_set_property(GObject* gobject, guint param_id,
                       const GValue* value, GParamSpec* pspec)
{
	g_return_if_fail(GANV_IS_ITEM(gobject));

	GanvItem* item = GANV_ITEM(gobject);

	switch (param_id) {
	case ITEM_PROP_PARENT:
		if (item->parent != NULL) {
			g_warning("Cannot set `parent' argument after item has "
			          "already been constructed.");
		} else if (g_value_get_object(value)) {
			item->parent = GANV_ITEM(g_value_get_object(value));
			item->canvas = item->parent->canvas;
			item_post_create_setup(item);
		}
		break;
	case ITEM_PROP_X:
		item->x = g_value_get_double(value);
		ganv_item_request_update(item);
		break;
	case ITEM_PROP_Y:
		item->y = g_value_get_double(value);
		ganv_item_request_update(item);
		break;
	case ITEM_PROP_MANAGED:
		item->managed = g_value_get_boolean(value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(gobject, param_id, pspec);
		break;
	}
}

/* Get_property handler for canvas items */
static void
ganv_item_get_property(GObject* gobject, guint param_id,
                       GValue* value, GParamSpec* pspec)
{
	g_return_if_fail(GANV_IS_ITEM(gobject));

	GanvItem* item = GANV_ITEM(gobject);

	switch (param_id) {
	case ITEM_PROP_PARENT:
		g_value_set_object(value, item->parent);
		break;
	case ITEM_PROP_X:
		g_value_set_double(value, item->x);
		break;
	case ITEM_PROP_Y:
		g_value_set_double(value, item->y);
		break;
	case ITEM_PROP_MANAGED:
		g_value_set_boolean(value, item->managed);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(gobject, param_id, pspec);
		break;
	}
}

/**
 * ganv_item_construct:
 * @item: An unconstructed canvas item.
 * @parent: The parent group for the item.
 * @first_arg_name: The name of the first argument for configuring the item.
 * @args: The list of arguments used to configure the item.
 *
 * Constructs a canvas item; meant for use only by item implementations.
 **/
void
ganv_item_construct(GanvItem* item, GanvItem* parent,
                    const gchar* first_arg_name, va_list args)
{
	g_return_if_fail(GANV_IS_ITEM(item));

	item->parent = parent;
	item->canvas = item->parent->canvas;
	item->layer = 0;

	g_object_set_valist(G_OBJECT(item), first_arg_name, args);

	item_post_create_setup(item);
}

/* If the item is visible, requests a redraw of it. */
static void
redraw_if_visible(GanvItem* item)
{
	if (item->object.flags & GANV_ITEM_VISIBLE) {
		ganv_canvas_base_request_redraw(item->canvas, item->x1, item->y1, item->x2 + 1,
		                                item->y2 + 1);
	}
}

/* Standard object dispose function for canvas items */
static void
ganv_item_dispose(GObject* object)
{
	GanvItem* item;

	g_return_if_fail(GANV_IS_ITEM(object));

	item = GANV_ITEM(object);

	if (item->canvas) {
		redraw_if_visible(item);
	}

	/* Make the canvas forget about us */

	if (item->canvas && item == item->canvas->current_item) {
		item->canvas->current_item = NULL;
		item->canvas->need_repick  = TRUE;
	}

	if (item->canvas && item == item->canvas->new_current_item) {
		item->canvas->new_current_item = NULL;
		item->canvas->need_repick      = TRUE;
	}

	if (item->canvas && item == item->canvas->grabbed_item) {
		item->canvas->grabbed_item = NULL;
		gdk_pointer_ungrab(GDK_CURRENT_TIME);
	}

	if (item->canvas && item == item->canvas->focused_item) {
		item->canvas->focused_item = NULL;
	}

	/* Normal destroy stuff */

	if (item->object.flags & GANV_ITEM_MAPPED) {
		(*GANV_ITEM_GET_CLASS(item)->unmap)(item);
	}

	if (item->object.flags & GANV_ITEM_REALIZED) {
		(*GANV_ITEM_GET_CLASS(item)->unrealize)(item);
	}

	if (!item->managed && item->parent) {
		if (GANV_ITEM_GET_CLASS(item->parent)->remove) {
			GANV_ITEM_GET_CLASS(item->parent)->remove(item->parent, item);
		} else {
			fprintf(stderr, "warning: Item parent has no remove method\n");
		}
	}

	G_OBJECT_CLASS(item_parent_class)->dispose(object);
	/* items should remove any reference to item->canvas after the
	   first ::destroy */
	item->canvas = NULL;
}

/* Realize handler for canvas items */
static void
ganv_item_realize(GanvItem* item)
{
	GTK_OBJECT_SET_FLAGS(item, GANV_ITEM_REALIZED);

	ganv_item_request_update(item);
}

/* Unrealize handler for canvas items */
static void
ganv_item_unrealize(GanvItem* item)
{
	GTK_OBJECT_UNSET_FLAGS(item, GANV_ITEM_REALIZED);
}

/* Map handler for canvas items */
static void
ganv_item_map(GanvItem* item)
{
	GTK_OBJECT_SET_FLAGS(item, GANV_ITEM_MAPPED);
}

/* Unmap handler for canvas items */
static void
ganv_item_unmap(GanvItem* item)
{
	GTK_OBJECT_UNSET_FLAGS(item, GANV_ITEM_MAPPED);
}

/* Update handler for canvas items */
static void
ganv_item_update(GanvItem* item, int flags)
{
	GTK_OBJECT_UNSET_FLAGS(item, GANV_ITEM_NEED_UPDATE);
	GTK_OBJECT_UNSET_FLAGS(item, GANV_ITEM_NEED_VIS);
}

/* Point handler for canvas items */
static double
ganv_item_point(GanvItem* item,
                double x, double y,
                int cx, int cy,
                GanvItem** actual_item)
{
	*actual_item = NULL;
	return G_MAXDOUBLE;
}

void
ganv_item_invoke_update(GanvItem* item, int flags)
{
	int child_flags = flags;

	/* apply object flags to child flags */

	child_flags &= ~GANV_CANVAS_BASE_UPDATE_REQUESTED;

	if (item->object.flags & GANV_ITEM_NEED_UPDATE) {
		child_flags |= GANV_CANVAS_BASE_UPDATE_REQUESTED;
	}

	if (item->object.flags & GANV_ITEM_NEED_VIS) {
		child_flags |= GANV_CANVAS_BASE_UPDATE_VISIBILITY;
	}

	if (child_flags & GCI_UPDATE_MASK) {
		if (GANV_ITEM_GET_CLASS(item)->update) {
			GANV_ITEM_GET_CLASS(item)->update(item, child_flags);
		}
	}
}

/**
 * ganv_item_set:
 * @item: A canvas item.
 * @first_arg_name: The list of object argument name/value pairs used to configure the item.
 * @Varargs:
 *
 * Configures a canvas item.  The arguments in the item are set to the specified
 * values, and the item is repainted as appropriate.
 **/
void
ganv_item_set(GanvItem* item, const gchar* first_arg_name, ...)
{
	va_list args;

	va_start(args, first_arg_name);
	ganv_item_set_valist(item, first_arg_name, args);
	va_end(args);
}

/**
 * ganv_item_set_valist:
 * @item: A canvas item.
 * @first_arg_name: The name of the first argument used to configure the item.
 * @args: The list of object argument name/value pairs used to configure the item.
 *
 * Configures a canvas item.  The arguments in the item are set to the specified
 * values, and the item is repainted as appropriate.
 **/
void
ganv_item_set_valist(GanvItem* item, const gchar* first_arg_name, va_list args)
{
	g_return_if_fail(GANV_IS_ITEM(item));

	g_object_set_valist(G_OBJECT(item), first_arg_name, args);

	item->canvas->need_repick = TRUE;
}

void
ganv_item_raise(GanvItem* item)
{
	++item->layer;
}

void
ganv_item_lower(GanvItem* item)
{
	--item->layer;
}

/**
 * ganv_item_move:
 * @item: A canvas item.
 * @dx: Horizontal offset.
 * @dy: Vertical offset.
 **/
void
ganv_item_move(GanvItem* item, double dx, double dy)
{
	g_return_if_fail(item != NULL);
	g_return_if_fail(GANV_IS_ITEM(item));

	item->x += dx;
	item->y += dy;

	ganv_item_request_update(item);
	item->canvas->need_repick = TRUE;
}

/**
 * ganv_item_show:
 * @item: A canvas item.
 *
 * Shows a canvas item.  If the item was already shown, then no action is taken.
 **/
void
ganv_item_show(GanvItem* item)
{
	g_return_if_fail(GANV_IS_ITEM(item));

	if (!(item->object.flags & GANV_ITEM_VISIBLE)) {
		item->object.flags |= GANV_ITEM_VISIBLE;
		ganv_canvas_base_request_redraw(item->canvas, item->x1, item->y1, item->x2 + 1,
		                                item->y2 + 1);
		item->canvas->need_repick = TRUE;
	}
}

/**
 * ganv_item_hide:
 * @item: A canvas item.
 *
 * Hides a canvas item.  If the item was already hidden, then no action is
 * taken.
 **/
void
ganv_item_hide(GanvItem* item)
{
	g_return_if_fail(GANV_IS_ITEM(item));

	if (item->object.flags & GANV_ITEM_VISIBLE) {
		item->object.flags &= ~GANV_ITEM_VISIBLE;
		ganv_canvas_base_request_redraw(item->canvas, item->x1, item->y1, item->x2 + 1,
		                                item->y2 + 1);
		item->canvas->need_repick = TRUE;
	}
}

/**
 * ganv_item_grab:
 * @item: A canvas item.
 * @event_mask: Mask of events that will be sent to this item.
 * @cursor: If non-NULL, the cursor that will be used while the grab is active.
 * @etime: The timestamp required for grabbing the mouse, or GDK_CURRENT_TIME.
 *
 * Specifies that all events that match the specified event mask should be sent
 * to the specified item, and also grabs the mouse by calling
 * gdk_pointer_grab().  The event mask is also used when grabbing the pointer.
 * If @cursor is not NULL, then that cursor is used while the grab is active.
 * The @etime parameter is the timestamp required for grabbing the mouse.
 *
 * Return value: If an item was already grabbed, it returns %GDK_GRAB_ALREADY_GRABBED.  If
 * the specified item was hidden by calling ganv_item_hide(), then it
 * returns %GDK_GRAB_NOT_VIEWABLE.  Else, it returns the result of calling
 * gdk_pointer_grab().
 **/
int
ganv_item_grab(GanvItem* item, guint event_mask, GdkCursor* cursor, guint32 etime)
{
	int retval;

	g_return_val_if_fail(GANV_IS_ITEM(item), GDK_GRAB_NOT_VIEWABLE);
	g_return_val_if_fail(GTK_WIDGET_MAPPED(item->canvas), GDK_GRAB_NOT_VIEWABLE);

	if (item->canvas->grabbed_item) {
		return GDK_GRAB_ALREADY_GRABBED;
	}

	if (!(item->object.flags & GANV_ITEM_VISIBLE)) {
		return GDK_GRAB_NOT_VIEWABLE;
	}

	retval = gdk_pointer_grab(item->canvas->layout.bin_window,
	                          FALSE,
	                          event_mask,
	                          NULL,
	                          cursor,
	                          etime);

	if (retval != GDK_GRAB_SUCCESS) {
		return retval;
	}

	item->canvas->grabbed_item       = item;
	item->canvas->grabbed_event_mask = event_mask;
	item->canvas->current_item       = item; /* So that events go to the grabbed item */

	return retval;
}

/**
 * ganv_item_ungrab:
 * @item: A canvas item that holds a grab.
 * @etime: The timestamp for ungrabbing the mouse.
 *
 * Ungrabs the item, which must have been grabbed in the canvas, and ungrabs the
 * mouse.
 **/
void
ganv_item_ungrab(GanvItem* item, guint32 etime)
{
	g_return_if_fail(GANV_IS_ITEM(item));

	if (item->canvas->grabbed_item != item) {
		return;
	}

	item->canvas->grabbed_item = NULL;

	gdk_pointer_ungrab(etime);
}

/**
 * ganv_item_i2w_affine:
 * @item: A canvas item
 * @matrix: An affine transformation matrix (return value).
 *
 * Gets the affine transform that converts from the item's coordinate system to
 * world coordinates.
 **/
void
ganv_item_i2w_affine(GanvItem* item, cairo_matrix_t* matrix)
{
	g_return_if_fail(GANV_IS_ITEM(item));

	cairo_matrix_init_identity(matrix);

	while (item) {
		matrix->x0 += item->x;
		matrix->y0 += item->y;
		item = item->parent;
	}
}

/**
 * ganv_item_w2i:
 * @item: A canvas item.
 * @x: X coordinate to convert (input/output value).
 * @y: Y coordinate to convert (input/output value).
 *
 * Converts a coordinate pair from world coordinates to item-relative
 * coordinates.
 **/
void
ganv_item_w2i(GanvItem* item, double* x, double* y)
{
	g_return_if_fail(GANV_IS_ITEM(item));
	g_return_if_fail(x != NULL);
	g_return_if_fail(y != NULL);

	cairo_matrix_t matrix;
	ganv_item_i2w_affine(item, &matrix);
	cairo_matrix_invert(&matrix);

	cairo_matrix_transform_point(&matrix, x, y);
}

/**
 * ganv_item_i2w:
 * @item: A canvas item.
 * @x: X coordinate to convert (input/output value).
 * @y: Y coordinate to convert (input/output value).
 *
 * Converts a coordinate pair from item-relative coordinates to world
 * coordinates.
 **/
void
ganv_item_i2w(GanvItem* item, double* x, double* y)
{
	g_return_if_fail(GANV_IS_ITEM(item));
	g_return_if_fail(x != NULL);
	g_return_if_fail(y != NULL);

	cairo_matrix_t matrix;
	ganv_item_i2w_affine(item, &matrix);

	cairo_matrix_transform_point(&matrix, x, y);
}

/**
 * ganv_item_i2c_affine:
 * @item: A canvas item.
 * @matrix: An affine transformation matrix (return value).
 *
 * Gets the affine transform that converts from item-relative coordinates to
 * canvas pixel coordinates.
 **/
void
ganv_item_i2c_affine(GanvItem* item, cairo_matrix_t* matrix)
{
	cairo_matrix_t i2w;
	ganv_item_i2w_affine(item, &i2w);

	cairo_matrix_t w2c;
	ganv_canvas_base_w2c_affine(item->canvas, &w2c);

	cairo_matrix_multiply(matrix, &i2w, &w2c);
}

/* Returns whether the item is an inferior of or is equal to the parent. */
static gboolean
is_descendant(GanvItem* item, GanvItem* parent)
{
	for (; item; item = item->parent) {
		if (item == parent) {
			return TRUE;
		}
	}

	return FALSE;
}

/**
 * ganv_item_grab_focus:
 * @item: A canvas item.
 *
 * Makes the specified item take the keyboard focus, so all keyboard events will
 * be sent to it.  If the canvas widget itself did not have the focus, it grabs
 * it as well.
 **/
void
ganv_item_grab_focus(GanvItem* item)
{
	GanvItem* focused_item;
	GdkEvent  ev;

	g_return_if_fail(GANV_IS_ITEM(item));
	g_return_if_fail(GTK_WIDGET_CAN_FOCUS(GTK_WIDGET(item->canvas)));

	focused_item = item->canvas->focused_item;

	if (focused_item) {
		ev.focus_change.type       = GDK_FOCUS_CHANGE;
		ev.focus_change.window     = GTK_LAYOUT(item->canvas)->bin_window;
		ev.focus_change.send_event = FALSE;
		ev.focus_change.in         = FALSE;

		emit_event(item->canvas, &ev);
	}

	item->canvas->focused_item = item;
	gtk_widget_grab_focus(GTK_WIDGET(item->canvas));

	if (focused_item) {
		ev.focus_change.type       = GDK_FOCUS_CHANGE;
		ev.focus_change.window     = GTK_LAYOUT(item->canvas)->bin_window;
		ev.focus_change.send_event = FALSE;
		ev.focus_change.in         = TRUE;

		emit_event(item->canvas, &ev);
	}
}

/**
 * ganv_item_get_bounds:
 * @item: A canvas item.
 * @x1: Leftmost edge of the bounding box (return value).
 * @y1: Upper edge of the bounding box (return value).
 * @x2: Rightmost edge of the bounding box (return value).
 * @y2: Lower edge of the bounding box (return value).
 *
 * Queries the bounding box of a canvas item.  The bounds are returned in the
 * coordinate system of the item's parent.
 **/
void
ganv_item_get_bounds(GanvItem* item, double* x1, double* y1, double* x2, double* y2)
{
	double ix1, iy1, ix2, iy2;

	g_return_if_fail(GANV_IS_ITEM(item));

	ix1 = iy1 = ix2 = iy2 = 0.0;

	/* Get the item's bounds in its coordinate system */

	if (GANV_ITEM_GET_CLASS(item)->bounds) {
		(*GANV_ITEM_GET_CLASS(item)->bounds)(item, &ix1, &iy1, &ix2, &iy2);
	}

	/* Make the bounds relative to the item's parent coordinate system */

	ix1 -= item->x;
	iy1 -= item->y;
	ix2 -= item->x;
	iy2 -= item->y;

	/* Return the values */

	if (x1) {
		*x1 = ix1;
	}

	if (y1) {
		*y1 = iy1;
	}

	if (x2) {
		*x2 = ix2;
	}

	if (y2) {
		*y2 = iy2;
	}
}

/**
 * ganv_item_request_update
 * @item: A canvas item.
 *
 * To be used only by item implementations.  Requests that the canvas queue an
 * update for the specified item.
 **/
void
ganv_item_request_update(GanvItem* item)
{
	/* FIXME: For some reason, if this short-circuit is enabled, the canvas
	   stops updating items entirely when connected modules are removed. */
	/*
	if (item->object.flags & GANV_ITEM_NEED_UPDATE) {
		return;
	}
	*/

	if (!item->canvas) {
		/* Item is being / has been destroyed, ignore */
		return;
	}

	item->object.flags |= GANV_ITEM_NEED_UPDATE;

	if (item->parent != NULL) {
		/* Recurse up the tree */
		ganv_item_request_update(item->parent);
	} else {
		/* Have reached the top of the tree, make sure the update call gets scheduled. */
		ganv_canvas_base_request_update(item->canvas);
	}
}

/*** GanvCanvasBase ***/

enum {
	DRAW_BACKGROUND,
	LAST_SIGNAL
};

static void ganv_canvas_base_destroy(GtkObject* object);
static void ganv_canvas_base_map(GtkWidget* widget);
static void ganv_canvas_base_unmap(GtkWidget* widget);
static void ganv_canvas_base_realize(GtkWidget* widget);
static void ganv_canvas_base_unrealize(GtkWidget* widget);
static void ganv_canvas_base_size_allocate(GtkWidget*     widget,
                                           GtkAllocation* allocation);
static gint ganv_canvas_base_button(GtkWidget*      widget,
                                    GdkEventButton* event);
static gint ganv_canvas_base_motion(GtkWidget*      widget,
                                    GdkEventMotion* event);
static gint ganv_canvas_base_expose(GtkWidget*      widget,
                                    GdkEventExpose* event);
static gboolean ganv_canvas_base_key(GtkWidget*   widget,
                                     GdkEventKey* event);
static gboolean ganv_canvas_base_scroll(GtkWidget*      widget,
                                        GdkEventScroll* event);
static gint ganv_canvas_base_crossing(GtkWidget*        widget,
                                      GdkEventCrossing* event);
static gint ganv_canvas_base_focus_in(GtkWidget*     widget,
                                      GdkEventFocus* event);
static gint ganv_canvas_base_focus_out(GtkWidget*     widget,
                                       GdkEventFocus* event);
static void ganv_canvas_base_request_update_real(GanvCanvasBase* canvas);
static void ganv_canvas_base_draw_background(GanvCanvasBase* canvas,
                                             GdkDrawable*    drawable,
                                             int             x,
                                             int             y,
                                             int             width,
                                             int             height);

static GtkLayoutClass* canvas_parent_class;

static guint canvas_signals[LAST_SIGNAL];

enum {
	PROP_FOCUSED_ITEM = 1
};

G_DEFINE_TYPE(GanvCanvasBase, ganv_canvas_base, GTK_TYPE_LAYOUT)

static void
ganv_canvas_base_get_property(GObject*    object,
                              guint       prop_id,
                              GValue*     value,
                              GParamSpec* pspec)
{
	switch (prop_id) {
	case PROP_FOCUSED_ITEM:
		g_value_set_object(value, GANV_CANVAS_BASE(object)->focused_item);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
ganv_canvas_base_set_property(GObject*      object,
                              guint         prop_id,
                              const GValue* value,
                              GParamSpec*   pspec)
{
	switch (prop_id) {
	case PROP_FOCUSED_ITEM:
		GANV_CANVAS_BASE(object)->focused_item = g_value_get_object(value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

/* Class initialization function for GanvCanvasBaseClass */
static void
ganv_canvas_base_class_init(GanvCanvasBaseClass* klass)
{
	GObjectClass*   gobject_class;
	GtkObjectClass* object_class;
	GtkWidgetClass* widget_class;

	gobject_class = (GObjectClass*)klass;
	object_class  = (GtkObjectClass*)klass;
	widget_class  = (GtkWidgetClass*)klass;

	canvas_parent_class = g_type_class_peek_parent(klass);

	gobject_class->set_property = ganv_canvas_base_set_property;
	gobject_class->get_property = ganv_canvas_base_get_property;

	object_class->destroy = ganv_canvas_base_destroy;

	widget_class->map                  = ganv_canvas_base_map;
	widget_class->unmap                = ganv_canvas_base_unmap;
	widget_class->realize              = ganv_canvas_base_realize;
	widget_class->unrealize            = ganv_canvas_base_unrealize;
	widget_class->size_allocate        = ganv_canvas_base_size_allocate;
	widget_class->button_press_event   = ganv_canvas_base_button;
	widget_class->button_release_event = ganv_canvas_base_button;
	widget_class->motion_notify_event  = ganv_canvas_base_motion;
	widget_class->expose_event         = ganv_canvas_base_expose;
	widget_class->key_press_event      = ganv_canvas_base_key;
	widget_class->key_release_event    = ganv_canvas_base_key;
	widget_class->enter_notify_event   = ganv_canvas_base_crossing;
	widget_class->leave_notify_event   = ganv_canvas_base_crossing;
	widget_class->focus_in_event       = ganv_canvas_base_focus_in;
	widget_class->focus_out_event      = ganv_canvas_base_focus_out;
	widget_class->scroll_event         = ganv_canvas_base_scroll;

	klass->draw_background = ganv_canvas_base_draw_background;
	klass->request_update  = ganv_canvas_base_request_update_real;

	g_object_class_install_property(gobject_class, PROP_FOCUSED_ITEM,
	                                g_param_spec_object("focused_item", NULL, NULL,
	                                                    GANV_TYPE_ITEM,
	                                                    (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	canvas_signals[DRAW_BACKGROUND]
	    = g_signal_new("draw_background",
	                   G_TYPE_FROM_CLASS(object_class),
	                   G_SIGNAL_RUN_LAST,
	                   G_STRUCT_OFFSET(GanvCanvasBaseClass, draw_background),
	                   NULL, NULL,
	                   ganv_marshal_VOID__OBJECT_INT_INT_INT_INT,
	                   G_TYPE_NONE, 5, GDK_TYPE_DRAWABLE,
	                   G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT);
}

/* Callback used when the root item of a canvas is destroyed.  The user should
 * never ever do this, so we panic if this happens.
 */
static void
panic_root_destroyed(GtkObject* object, gpointer data)
{
	g_error("Eeeek, root item %p of canvas %p was destroyed!", (void*)object, data);
}

/* Object initialization function for GanvCanvasBase */
static void
ganv_canvas_base_init(GanvCanvasBase* canvas)
{
	GTK_WIDGET_SET_FLAGS(canvas, GTK_CAN_FOCUS);

	canvas->need_update   = FALSE;
	canvas->need_redraw   = FALSE;
	canvas->redraw_region = NULL;
	canvas->idle_id       = 0;

	canvas->scroll_x1 = 0.0;
	canvas->scroll_y1 = 0.0;
	canvas->scroll_x2 = canvas->layout.width;
	canvas->scroll_y2 = canvas->layout.height;

	canvas->pixels_per_unit = 1.0;

	canvas->pick_event.type       = GDK_LEAVE_NOTIFY;
	canvas->pick_event.crossing.x = 0;
	canvas->pick_event.crossing.y = 0;

	canvas->center_scroll_region = TRUE;

	gtk_layout_set_hadjustment(GTK_LAYOUT(canvas), NULL);
	gtk_layout_set_vadjustment(GTK_LAYOUT(canvas), NULL);

	/* Create the root item as a special case */
	canvas->root         = GANV_ITEM(g_object_new(ganv_group_get_type(), NULL));
	canvas->root->canvas = canvas;

	g_object_ref_sink(canvas->root);

	canvas->root_destroy_id = g_signal_connect(canvas->root, "destroy",
	                                           G_CALLBACK(panic_root_destroyed),
	                                           canvas);

	canvas->need_repick = TRUE;
}

/* Convenience function to remove the idle handler of a canvas */
static void
remove_idle(GanvCanvasBase* canvas)
{
	if (canvas->idle_id == 0) {
		return;
	}

	g_source_remove(canvas->idle_id);
	canvas->idle_id = 0;
}

/* Removes the transient state of the canvas (idle handler, grabs). */
static void
shutdown_transients(GanvCanvasBase* canvas)
{
	/* We turn off the need_redraw flag, since if the canvas is mapped again
	 * it will request a redraw anyways.  We do not turn off the need_update
	 * flag, though, because updates are not queued when the canvas remaps
	 * itself.
	 */
	if (canvas->need_redraw) {
		canvas->need_redraw = FALSE;
		g_slist_foreach(canvas->redraw_region, (GFunc)g_free, NULL);
		g_slist_free(canvas->redraw_region);
		canvas->redraw_region = NULL;
		canvas->redraw_x1   = 0;
		canvas->redraw_y1   = 0;
		canvas->redraw_x2   = 0;
		canvas->redraw_y2   = 0;
	}

	if (canvas->grabbed_item) {
		canvas->grabbed_item = NULL;
		gdk_pointer_ungrab(GDK_CURRENT_TIME);
	}

	remove_idle(canvas);
}

/* Destroy handler for GanvCanvasBase */
static void
ganv_canvas_base_destroy(GtkObject* object)
{
	GanvCanvasBase* canvas;

	g_return_if_fail(GANV_IS_CANVAS_BASE(object));

	/* remember, destroy can be run multiple times! */

	canvas = GANV_CANVAS_BASE(object);

	if (canvas->root_destroy_id) {
		g_signal_handler_disconnect(canvas->root, canvas->root_destroy_id);
		canvas->root_destroy_id = 0;
	}
	if (canvas->root) {
		gtk_object_destroy(GTK_OBJECT(canvas->root));
		g_object_unref(G_OBJECT(canvas->root));
		canvas->root = NULL;
	}

	shutdown_transients(canvas);

	if (GTK_OBJECT_CLASS(canvas_parent_class)->destroy) {
		(*GTK_OBJECT_CLASS(canvas_parent_class)->destroy)(object);
	}
}

/**
 * ganv_canvas_base_new:
 *
 * Creates a new empty canvas in non-antialiased mode.
 *
 * Return value: A newly-created canvas.
 **/
GtkWidget*
ganv_canvas_base_new(void)
{
	return GTK_WIDGET(g_object_new(ganv_canvas_base_get_type(), NULL));
}

/* Map handler for the canvas */
static void
ganv_canvas_base_map(GtkWidget* widget)
{
	GanvCanvasBase* canvas;

	g_return_if_fail(GANV_IS_CANVAS_BASE(widget));

	/* Normal widget mapping stuff */

	if (GTK_WIDGET_CLASS(canvas_parent_class)->map) {
		(*GTK_WIDGET_CLASS(canvas_parent_class)->map)(widget);
	}

	canvas = GANV_CANVAS_BASE(widget);

	if (canvas->need_update) {
		add_idle(canvas);
	}

	/* Map items */

	if (GANV_ITEM_GET_CLASS(canvas->root)->map) {
		(*GANV_ITEM_GET_CLASS(canvas->root)->map)(canvas->root);
	}
}

/* Unmap handler for the canvas */
static void
ganv_canvas_base_unmap(GtkWidget* widget)
{
	GanvCanvasBase* canvas;

	g_return_if_fail(GANV_IS_CANVAS_BASE(widget));

	canvas = GANV_CANVAS_BASE(widget);

	shutdown_transients(canvas);

	/* Unmap items */

	if (GANV_ITEM_GET_CLASS(canvas->root)->unmap) {
		(*GANV_ITEM_GET_CLASS(canvas->root)->unmap)(canvas->root);
	}

	/* Normal widget unmapping stuff */

	if (GTK_WIDGET_CLASS(canvas_parent_class)->unmap) {
		(*GTK_WIDGET_CLASS(canvas_parent_class)->unmap)(widget);
	}
}

/* Realize handler for the canvas */
static void
ganv_canvas_base_realize(GtkWidget* widget)
{
	GanvCanvasBase* canvas;

	g_return_if_fail(GANV_IS_CANVAS_BASE(widget));

	/* Normal widget realization stuff */

	if (GTK_WIDGET_CLASS(canvas_parent_class)->realize) {
		(*GTK_WIDGET_CLASS(canvas_parent_class)->realize)(widget);
	}

	canvas = GANV_CANVAS_BASE(widget);

	gdk_window_set_events(canvas->layout.bin_window,
	                      (gdk_window_get_events(canvas->layout.bin_window)
	                       | GDK_EXPOSURE_MASK
	                       | GDK_BUTTON_PRESS_MASK
	                       | GDK_BUTTON_RELEASE_MASK
	                       | GDK_POINTER_MOTION_MASK
	                       | GDK_KEY_PRESS_MASK
	                       | GDK_KEY_RELEASE_MASK
	                       | GDK_ENTER_NOTIFY_MASK
	                       | GDK_LEAVE_NOTIFY_MASK
	                       | GDK_FOCUS_CHANGE_MASK));

	/* Create our own temporary pixmap gc and realize all the items */

	canvas->pixmap_gc = gdk_gc_new(canvas->layout.bin_window);

	(*GANV_ITEM_GET_CLASS(canvas->root)->realize)(canvas->root);
}

/* Unrealize handler for the canvas */
static void
ganv_canvas_base_unrealize(GtkWidget* widget)
{
	GanvCanvasBase* canvas;

	g_return_if_fail(GANV_IS_CANVAS_BASE(widget));

	canvas = GANV_CANVAS_BASE(widget);

	shutdown_transients(canvas);

	/* Unrealize items and parent widget */

	(*GANV_ITEM_GET_CLASS(canvas->root)->unrealize)(canvas->root);

	g_object_unref(canvas->pixmap_gc);
	canvas->pixmap_gc = NULL;

	if (GTK_WIDGET_CLASS(canvas_parent_class)->unrealize) {
		(*GTK_WIDGET_CLASS(canvas_parent_class)->unrealize)(widget);
	}
}

/* Handles scrolling of the canvas.  Adjusts the scrolling and zooming offset to
 * keep as much as possible of the canvas scrolling region in view.
 */
static void
scroll_to(GanvCanvasBase* canvas, int cx, int cy)
{
	int scroll_width, scroll_height;
	int right_limit, bottom_limit;
	int old_zoom_xofs, old_zoom_yofs;
	int changed_x = FALSE, changed_y = FALSE;
	int canvas_width, canvas_height;

	canvas_width  = GTK_WIDGET(canvas)->allocation.width;
	canvas_height = GTK_WIDGET(canvas)->allocation.height;

	scroll_width = floor((canvas->scroll_x2 - canvas->scroll_x1) * canvas->pixels_per_unit
	                     + 0.5);
	scroll_height = floor((canvas->scroll_y2 - canvas->scroll_y1) * canvas->pixels_per_unit
	                      + 0.5);

	right_limit  = scroll_width - canvas_width;
	bottom_limit = scroll_height - canvas_height;

	old_zoom_xofs = canvas->zoom_xofs;
	old_zoom_yofs = canvas->zoom_yofs;

	if (right_limit < 0) {
		cx = 0;

		if (canvas->center_scroll_region) {
			canvas->zoom_xofs = (canvas_width - scroll_width) / 2;
			scroll_width      = canvas_width;
		} else {
			canvas->zoom_xofs = 0;
		}
	} else if (cx < 0) {
		cx                = 0;
		canvas->zoom_xofs = 0;
	} else if (cx > right_limit) {
		cx                = right_limit;
		canvas->zoom_xofs = 0;
	} else {
		canvas->zoom_xofs = 0;
	}

	if (bottom_limit < 0) {
		cy = 0;

		if (canvas->center_scroll_region) {
			canvas->zoom_yofs = (canvas_height - scroll_height) / 2;
			scroll_height     = canvas_height;
		} else {
			canvas->zoom_yofs = 0;
		}
	} else if (cy < 0) {
		cy                = 0;
		canvas->zoom_yofs = 0;
	} else if (cy > bottom_limit) {
		cy                = bottom_limit;
		canvas->zoom_yofs = 0;
	} else {
		canvas->zoom_yofs = 0;
	}

	if ((canvas->zoom_xofs != old_zoom_xofs) || (canvas->zoom_yofs != old_zoom_yofs)) {
		ganv_canvas_base_request_update(canvas);
		gtk_widget_queue_draw(GTK_WIDGET(canvas));
	}

	if (canvas->layout.hadjustment && ( ((int)canvas->layout.hadjustment->value) != cx) ) {
		canvas->layout.hadjustment->value = cx;
		changed_x                         = TRUE;
	}

	if (canvas->layout.vadjustment && ( ((int)canvas->layout.vadjustment->value) != cy) ) {
		canvas->layout.vadjustment->value = cy;
		changed_y                         = TRUE;
	}

	if ((scroll_width != (int)canvas->layout.width)
	    || (scroll_height != (int)canvas->layout.height)) {
		gtk_layout_set_size(GTK_LAYOUT(canvas), scroll_width, scroll_height);
	}

	/* Signal GtkLayout that it should do a redraw. */

	if (changed_x) {
		g_signal_emit_by_name(canvas->layout.hadjustment, "value_changed");
	}

	if (changed_y) {
		g_signal_emit_by_name(canvas->layout.vadjustment, "value_changed");
	}
}

/* Size allocation handler for the canvas */
static void
ganv_canvas_base_size_allocate(GtkWidget* widget, GtkAllocation* allocation)
{
	GanvCanvasBase* canvas;

	g_return_if_fail(GANV_IS_CANVAS_BASE(widget));
	g_return_if_fail(allocation != NULL);

	if (GTK_WIDGET_CLASS(canvas_parent_class)->size_allocate) {
		(*GTK_WIDGET_CLASS(canvas_parent_class)->size_allocate)(widget, allocation);
	}

	canvas = GANV_CANVAS_BASE(widget);

	/* Recenter the view, if appropriate */

	canvas->layout.hadjustment->page_size      = allocation->width;
	canvas->layout.hadjustment->page_increment = allocation->width / 2;

	canvas->layout.vadjustment->page_size      = allocation->height;
	canvas->layout.vadjustment->page_increment = allocation->height / 2;

	scroll_to(canvas,
	          canvas->layout.hadjustment->value,
	          canvas->layout.vadjustment->value);

	g_signal_emit_by_name(canvas->layout.hadjustment, "changed");
	g_signal_emit_by_name(canvas->layout.vadjustment, "changed");
}

/* Emits an event for an item in the canvas, be it the current item, grabbed
 * item, or focused item, as appropriate.
 */

static int
emit_event(GanvCanvasBase* canvas, GdkEvent* event)
{
	GdkEvent* ev;
	gint      finished;
	GanvItem* item;
	GanvItem* parent;
	guint     mask;

	/* Perform checks for grabbed items */

	if (canvas->grabbed_item
	    && !is_descendant(canvas->current_item, canvas->grabbed_item)) {
		/* I think this warning is annoying and I don't know what it's for
		 * so I'll disable it for now.
		 */
		/*                g_warning ("emit_event() returning FALSE!\n");*/
		return FALSE;
	}

	if (canvas->grabbed_item) {
		switch (event->type) {
		case GDK_ENTER_NOTIFY:
			mask = GDK_ENTER_NOTIFY_MASK;
			break;

		case GDK_LEAVE_NOTIFY:
			mask = GDK_LEAVE_NOTIFY_MASK;
			break;

		case GDK_MOTION_NOTIFY:
			mask = GDK_POINTER_MOTION_MASK;
			break;

		case GDK_BUTTON_PRESS:
		case GDK_2BUTTON_PRESS:
		case GDK_3BUTTON_PRESS:
			mask = GDK_BUTTON_PRESS_MASK;
			break;

		case GDK_BUTTON_RELEASE:
			mask = GDK_BUTTON_RELEASE_MASK;
			break;

		case GDK_KEY_PRESS:
			mask = GDK_KEY_PRESS_MASK;
			break;

		case GDK_KEY_RELEASE:
			mask = GDK_KEY_RELEASE_MASK;
			break;

		case GDK_SCROLL:
			mask = GDK_SCROLL_MASK;
			break;

		default:
			mask = 0;
			break;
		}

		if (!(mask & canvas->grabbed_event_mask)) {
			return FALSE;
		}
	}

	/* Convert to world coordinates -- we have two cases because of diferent
	 * offsets of the fields in the event structures.
	 */

	ev = gdk_event_copy(event);

	switch (ev->type) {
	case GDK_ENTER_NOTIFY:
	case GDK_LEAVE_NOTIFY:
		ganv_canvas_base_window_to_world(canvas,
		                                 ev->crossing.x, ev->crossing.y,
		                                 &ev->crossing.x, &ev->crossing.y);
		break;

	case GDK_MOTION_NOTIFY:
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
	case GDK_BUTTON_RELEASE:
		ganv_canvas_base_window_to_world(canvas,
		                                 ev->motion.x, ev->motion.y,
		                                 &ev->motion.x, &ev->motion.y);
		break;

	default:
		break;
	}

	/* Choose where we send the event */

	item = canvas->current_item;

	if (canvas->focused_item
	    && ((event->type == GDK_KEY_PRESS)
	        || (event->type == GDK_KEY_RELEASE)
	        || (event->type == GDK_FOCUS_CHANGE))) {
		item = canvas->focused_item;
	}

	/* The event is propagated up the hierarchy (for if someone connected to
	 * a group instead of a leaf event), and emission is stopped if a
	 * handler returns TRUE, just like for GtkWidget events.
	 */

	finished = FALSE;

	while (item && !finished) {
		g_object_ref(G_OBJECT(item));

		g_signal_emit(item, item_signals[ITEM_EVENT], 0,
		              ev, &finished);

		parent = item->parent;
		g_object_unref(G_OBJECT(item));

		item = parent;
	}

	gdk_event_free(ev);

	return finished;
}

/* Re-picks the current item in the canvas, based on the event's coordinates.
 * Also emits enter/leave events for items as appropriate.
 */
static int
pick_current_item(GanvCanvasBase* canvas, GdkEvent* event)
{
	int    button_down;
	double x, y;
	int    cx, cy;
	int    retval;

	retval = FALSE;

	/* If a button is down, we'll perform enter and leave events on the
	 * current item, but not enter on any other item.  This is more or less
	 * like X pointer grabbing for canvas items.
	 */
	button_down = canvas->state & (GDK_BUTTON1_MASK
	                               | GDK_BUTTON2_MASK
	                               | GDK_BUTTON3_MASK
	                               | GDK_BUTTON4_MASK
	                               | GDK_BUTTON5_MASK);
	if (!button_down) {
		canvas->left_grabbed_item = FALSE;
	}

	/* Save the event in the canvas.  This is used to synthesize enter and
	 * leave events in case the current item changes.  It is also used to
	 * re-pick the current item if the current one gets deleted.  Also,
	 * synthesize an enter event.
	 */
	if (event != &canvas->pick_event) {
		if ((event->type == GDK_MOTION_NOTIFY) || (event->type == GDK_BUTTON_RELEASE)) {
			/* these fields have the same offsets in both types of events */

			canvas->pick_event.crossing.type       = GDK_ENTER_NOTIFY;
			canvas->pick_event.crossing.window     = event->motion.window;
			canvas->pick_event.crossing.send_event = event->motion.send_event;
			canvas->pick_event.crossing.subwindow  = NULL;
			canvas->pick_event.crossing.x          = event->motion.x;
			canvas->pick_event.crossing.y          = event->motion.y;
			canvas->pick_event.crossing.mode       = GDK_CROSSING_NORMAL;
			canvas->pick_event.crossing.detail     = GDK_NOTIFY_NONLINEAR;
			canvas->pick_event.crossing.focus      = FALSE;
			canvas->pick_event.crossing.state      = event->motion.state;

			/* these fields don't have the same offsets in both types of events */

			if (event->type == GDK_MOTION_NOTIFY) {
				canvas->pick_event.crossing.x_root = event->motion.x_root;
				canvas->pick_event.crossing.y_root = event->motion.y_root;
			} else {
				canvas->pick_event.crossing.x_root = event->button.x_root;
				canvas->pick_event.crossing.y_root = event->button.y_root;
			}
		} else {
			canvas->pick_event = *event;
		}
	}

	/* Don't do anything else if this is a recursive call */

	if (canvas->in_repick) {
		return retval;
	}

	/* LeaveNotify means that there is no current item, so we don't look for one */

	if (canvas->pick_event.type != GDK_LEAVE_NOTIFY) {
		/* these fields don't have the same offsets in both types of events */

		if (canvas->pick_event.type == GDK_ENTER_NOTIFY) {
			x = canvas->pick_event.crossing.x - canvas->zoom_xofs;
			y = canvas->pick_event.crossing.y - canvas->zoom_yofs;
		} else {
			x = canvas->pick_event.motion.x - canvas->zoom_xofs;
			y = canvas->pick_event.motion.y - canvas->zoom_yofs;
		}

		/* canvas pixel coords */

		cx = (int)(x + 0.5);
		cy = (int)(y + 0.5);

		/* world coords */

		x = canvas->scroll_x1 + x / canvas->pixels_per_unit;
		y = canvas->scroll_y1 + y / canvas->pixels_per_unit;

		/* find the closest item */

		if (canvas->root->object.flags & GANV_ITEM_VISIBLE) {
			GANV_ITEM_GET_CLASS(canvas->root)->point(
				canvas->root,
				x - canvas->root->x, y - canvas->root->y,
				cx, cy,
				&canvas->new_current_item);
		} else {
			canvas->new_current_item = NULL;
		}
	} else {
		canvas->new_current_item = NULL;
	}

	if ((canvas->new_current_item == canvas->current_item) && !canvas->left_grabbed_item) {
		return retval; /* current item did not change */

	}
	/* Synthesize events for old and new current items */

	if ((canvas->new_current_item != canvas->current_item)
	    && (canvas->current_item != NULL)
	    && !canvas->left_grabbed_item) {
		GdkEvent new_event;

		new_event      = canvas->pick_event;
		new_event.type = GDK_LEAVE_NOTIFY;

		new_event.crossing.detail    = GDK_NOTIFY_ANCESTOR;
		new_event.crossing.subwindow = NULL;
		canvas->in_repick            = TRUE;
		retval                       = emit_event(canvas, &new_event);
		canvas->in_repick            = FALSE;
	}

	/* new_current_item may have been set to NULL during the call to emit_event() above */

	if ((canvas->new_current_item != canvas->current_item) && button_down) {
		canvas->left_grabbed_item = TRUE;
		return retval;
	}

	/* Handle the rest of cases */

	canvas->left_grabbed_item = FALSE;
	canvas->current_item      = canvas->new_current_item;

	if (canvas->current_item != NULL) {
		GdkEvent new_event;

		new_event                    = canvas->pick_event;
		new_event.type               = GDK_ENTER_NOTIFY;
		new_event.crossing.detail    = GDK_NOTIFY_ANCESTOR;
		new_event.crossing.subwindow = NULL;
		retval                       = emit_event(canvas, &new_event);
	}

	return retval;
}

/* Button event handler for the canvas */
static gint
ganv_canvas_base_button(GtkWidget* widget, GdkEventButton* event)
{
	GanvCanvasBase* canvas;
	int             mask;
	int             retval;

	g_return_val_if_fail(GANV_IS_CANVAS_BASE(widget), FALSE);
	g_return_val_if_fail(event != NULL, FALSE);

	retval = FALSE;

	canvas = GANV_CANVAS_BASE(widget);

	/*
	 * dispatch normally regardless of the event's window if an item has
	 * has a pointer grab in effect
	 */
	if (!canvas->grabbed_item && ( event->window != canvas->layout.bin_window) ) {
		return retval;
	}

	switch (event->button) {
	case 1:
		mask = GDK_BUTTON1_MASK;
		break;
	case 2:
		mask = GDK_BUTTON2_MASK;
		break;
	case 3:
		mask = GDK_BUTTON3_MASK;
		break;
	case 4:
		mask = GDK_BUTTON4_MASK;
		break;
	case 5:
		mask = GDK_BUTTON5_MASK;
		break;
	default:
		mask = 0;
	}

	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		/* Pick the current item as if the button were not pressed, and
		 * then process the event.
		 */
		canvas->state = event->state;
		pick_current_item(canvas, (GdkEvent*)event);
		canvas->state ^= mask;
		retval         = emit_event(canvas, (GdkEvent*)event);
		break;

	case GDK_BUTTON_RELEASE:
		/* Process the event as if the button were pressed, then repick
		 * after the button has been released
		 */
		canvas->state = event->state;
		retval        = emit_event(canvas, (GdkEvent*)event);
		event->state ^= mask;
		canvas->state = event->state;
		pick_current_item(canvas, (GdkEvent*)event);
		event->state ^= mask;
		break;

	default:
		g_assert_not_reached();
	}

	return retval;
}

/* Motion event handler for the canvas */
static gint
ganv_canvas_base_motion(GtkWidget* widget, GdkEventMotion* event)
{
	GanvCanvasBase* canvas;

	g_return_val_if_fail(GANV_IS_CANVAS_BASE(widget), FALSE);
	g_return_val_if_fail(event != NULL, FALSE);

	canvas = GANV_CANVAS_BASE(widget);

	if (event->window != canvas->layout.bin_window) {
		return FALSE;
	}

	canvas->state = event->state;
	pick_current_item(canvas, (GdkEvent*)event);
	return emit_event(canvas, (GdkEvent*)event);
}

static gboolean
ganv_canvas_base_scroll(GtkWidget* widget, GdkEventScroll* event)
{
	GanvCanvasBase* canvas;

	g_return_val_if_fail(GANV_IS_CANVAS_BASE(widget), FALSE);
	g_return_val_if_fail(event != NULL, FALSE);

	canvas = GANV_CANVAS_BASE(widget);

	if (event->window != canvas->layout.bin_window) {
		return FALSE;
	}

	canvas->state = event->state;
	pick_current_item(canvas, (GdkEvent*)event);
	return emit_event(canvas, (GdkEvent*)event);
}

/* Key event handler for the canvas */
static gboolean
ganv_canvas_base_key(GtkWidget* widget, GdkEventKey* event)
{
	GanvCanvasBase* canvas;

	g_return_val_if_fail(GANV_IS_CANVAS_BASE(widget), FALSE);
	g_return_val_if_fail(event != NULL, FALSE);

	canvas = GANV_CANVAS_BASE(widget);

	if (!emit_event(canvas, (GdkEvent*)event)) {
		GtkWidgetClass* widget_class;

		widget_class = GTK_WIDGET_CLASS(canvas_parent_class);

		if (event->type == GDK_KEY_PRESS) {
			if (widget_class->key_press_event) {
				return (*widget_class->key_press_event)(widget, event);
			}
		} else if (event->type == GDK_KEY_RELEASE) {
			if (widget_class->key_release_event) {
				return (*widget_class->key_release_event)(widget, event);
			}
		} else {
			g_assert_not_reached();
		}

		return FALSE;
	} else {
		return TRUE;
	}
}

/* Crossing event handler for the canvas */
static gint
ganv_canvas_base_crossing(GtkWidget* widget, GdkEventCrossing* event)
{
	GanvCanvasBase* canvas;

	g_return_val_if_fail(GANV_IS_CANVAS_BASE(widget), FALSE);
	g_return_val_if_fail(event != NULL, FALSE);

	canvas = GANV_CANVAS_BASE(widget);

	if (event->window != canvas->layout.bin_window) {
		return FALSE;
	}

	canvas->state = event->state;
	return pick_current_item(canvas, (GdkEvent*)event);
}

/* Focus in handler for the canvas */
static gint
ganv_canvas_base_focus_in(GtkWidget* widget, GdkEventFocus* event)
{
	GanvCanvasBase* canvas;

	GTK_WIDGET_SET_FLAGS(widget, GTK_HAS_FOCUS);

	canvas = GANV_CANVAS_BASE(widget);

	if (canvas->focused_item) {
		return emit_event(canvas, (GdkEvent*)event);
	} else {
		return FALSE;
	}
}

/* Focus out handler for the canvas */
static gint
ganv_canvas_base_focus_out(GtkWidget* widget, GdkEventFocus* event)
{
	GanvCanvasBase* canvas;

	GTK_WIDGET_UNSET_FLAGS(widget, GTK_HAS_FOCUS);

	canvas = GANV_CANVAS_BASE(widget);

	if (canvas->focused_item) {
		return emit_event(canvas, (GdkEvent*)event);
	} else {
		return FALSE;
	}
}

#define REDRAW_QUANTUM_SIZE 512

static void
ganv_canvas_base_paint_rect(GanvCanvasBase* canvas, gint x0, gint y0, gint x1, gint y1)
{
	gint draw_x1, draw_y1;
	gint draw_x2, draw_y2;
	gint draw_width, draw_height;

	g_return_if_fail(!canvas->need_update);

	draw_x1 = MAX(x0, canvas->layout.hadjustment->value - canvas->zoom_xofs);
	draw_y1 = MAX(y0, canvas->layout.vadjustment->value - canvas->zoom_yofs);
	draw_x2 = MIN(draw_x1 + GTK_WIDGET(canvas)->allocation.width, x1);
	draw_y2 = MIN(draw_y1 + GTK_WIDGET(canvas)->allocation.height, y1);

	draw_width  = draw_x2 - draw_x1;
	draw_height = draw_y2 - draw_y1;

	if (( draw_width < 1) || ( draw_height < 1) ) {
		return;
	}

	canvas->redraw_x1 = draw_x1;
	canvas->redraw_y1 = draw_y1;
	canvas->redraw_x2 = draw_x2;
	canvas->redraw_y2 = draw_y2;
	canvas->draw_xofs = draw_x1;
	canvas->draw_yofs = draw_y1;

	cairo_t* cr = gdk_cairo_create(canvas->layout.bin_window);

	double wx, wy;
	ganv_canvas_base_window_to_world(canvas, 0, 0, &wx, &wy);
	cairo_translate(cr, -wx, -wy);

	if (canvas->root->object.flags & GANV_ITEM_VISIBLE) {
		(*GANV_ITEM_GET_CLASS(canvas->root)->draw)(
		    canvas->root, cr,
		    draw_x1, draw_y1,
		    draw_width, draw_height);
	}

	cairo_destroy(cr);
}

/* Expose handler for the canvas */
static gint
ganv_canvas_base_expose(GtkWidget* widget, GdkEventExpose* event)
{
	GanvCanvasBase* canvas;
	GdkRectangle*   rects;
	gint            n_rects;
	int             i;

	canvas = GANV_CANVAS_BASE(widget);

	if (!GTK_WIDGET_DRAWABLE(widget) || (event->window != canvas->layout.bin_window)) {
		return FALSE;
	}

#ifdef VERBOSE
	g_print("Expose\n");
#endif

	gdk_region_get_rectangles(event->region, &rects, &n_rects);

	for (i = 0; i < n_rects; i++) {
		const int x0 = rects[i].x - canvas->zoom_xofs;
		const int y0 = rects[i].y - canvas->zoom_yofs;
		const int x1 = rects[i].x + rects[i].width - canvas->zoom_xofs;
		const int y1 = rects[i].y + rects[i].height - canvas->zoom_yofs;

		if (canvas->need_update || canvas->need_redraw) {
			/* Update or drawing is scheduled, so just mark exposed area as dirty */
			ganv_canvas_base_request_redraw(canvas, x0, y0, x1, y1);
		} else {
			/* No pending updates, draw exposed area immediately */
			ganv_canvas_base_paint_rect(canvas, x0, y0, x1, y1);

			/* And call expose on parent container class */
			if (GTK_WIDGET_CLASS(canvas_parent_class)->expose_event) {
				(*GTK_WIDGET_CLASS(canvas_parent_class)->expose_event)(
				    widget, event);
			}
		}
	}

	g_free(rects);

	return FALSE;
}

/* Repaints the areas in the canvas that need it */
static void
paint(GanvCanvasBase* canvas)
{
	for (GSList* l = canvas->redraw_region; l; l = l->next) {
		IRect* rect = (IRect*)l->data;

		const GdkRectangle gdkrect = {
			rect->x + canvas->zoom_xofs,
			rect->y + canvas->zoom_yofs,
			rect->width,
			rect->height
		};

		gdk_window_invalidate_rect(canvas->layout.bin_window, &gdkrect, FALSE);
		g_free(rect);
	}

	g_slist_free(canvas->redraw_region);
	canvas->redraw_region = NULL;
	canvas->need_redraw = FALSE;

	canvas->redraw_x1 = 0;
	canvas->redraw_y1 = 0;
	canvas->redraw_x2 = 0;
	canvas->redraw_y2 = 0;
}

static void
ganv_canvas_base_draw_background(GanvCanvasBase* canvas, GdkDrawable* drawable,
                                 int x, int y, int width, int height)
{
	/* By default, we use the style background. */
	gdk_gc_set_foreground(canvas->pixmap_gc,
	                      &GTK_WIDGET(canvas)->style->bg[GTK_STATE_NORMAL]);
	gdk_draw_rectangle(drawable,
	                   canvas->pixmap_gc,
	                   TRUE,
	                   0, 0,
	                   width, height);
}

static void
do_update(GanvCanvasBase* canvas)
{
	/* Cause the update if necessary */

update_again:
	if (canvas->need_update) {
		ganv_item_invoke_update(canvas->root, 0);

		canvas->need_update = FALSE;
	}

	/* Pick new current item */

	while (canvas->need_repick) {
		canvas->need_repick = FALSE;
		pick_current_item(canvas, &canvas->pick_event);
	}

	/* it is possible that during picking we emitted an event in which
	   the user then called some function which then requested update
	   of something.  Without this we'd be left in a state where
	   need_update would have been left TRUE and the canvas would have
	   been left unpainted. */
	if (canvas->need_update) {
		goto update_again;
	}

	/* Paint if able to */

	if (GTK_WIDGET_DRAWABLE(canvas) && canvas->need_redraw) {
		paint(canvas);
	}
}

/* Idle handler for the canvas.  It deals with pending updates and redraws. */
static gboolean
idle_handler(gpointer data)
{
	GanvCanvasBase* canvas;

	GDK_THREADS_ENTER();

	canvas = GANV_CANVAS_BASE(data);

	do_update(canvas);

	/* Reset idle id */
	canvas->idle_id = 0;

	GDK_THREADS_LEAVE();

	return FALSE;
}

/* Convenience function to add an idle handler to a canvas */
static void
add_idle(GanvCanvasBase* canvas)
{
	g_assert(canvas->need_update || canvas->need_redraw);

	if (!canvas->idle_id) {
		canvas->idle_id = g_idle_add_full(CANVAS_IDLE_PRIORITY,
		                                  idle_handler,
		                                  canvas,
		                                  NULL);
	}

	/*      canvas->idle_id = gtk_idle_add (idle_handler, canvas); */
}

/**
 * ganv_canvas_base_root:
 * @canvas: A canvas.
 *
 * Queries the root group of a canvas.
 *
 * Return value: (transfer none): The root group of the specified canvas.
 **/
GanvItem*
ganv_canvas_base_root(GanvCanvasBase* canvas)
{
	g_return_val_if_fail(GANV_IS_CANVAS_BASE(canvas), NULL);

	return canvas->root;
}

/**
 * ganv_canvas_base_set_scroll_region:
 * @canvas: A canvas.
 * @x1: Leftmost limit of the scrolling region.
 * @y1: Upper limit of the scrolling region.
 * @x2: Rightmost limit of the scrolling region.
 * @y2: Lower limit of the scrolling region.
 *
 * Sets the scrolling region of a canvas to the specified rectangle.  The canvas
 * will then be able to scroll only within this region.  The view of the canvas
 * is adjusted as appropriate to display as much of the new region as possible.
 **/
void
ganv_canvas_base_set_scroll_region(GanvCanvasBase* canvas, double x1, double y1, double x2,
                                   double y2)
{
	double wxofs, wyofs;
	int    xofs, yofs;

	g_return_if_fail(GANV_IS_CANVAS_BASE(canvas));

	/*
	 * Set the new scrolling region.  If possible, do not move the visible contents of the
	 * canvas.
	 */

	ganv_canvas_base_c2w(canvas,
	                     GTK_LAYOUT(canvas)->hadjustment->value + canvas->zoom_xofs,
	                     GTK_LAYOUT(canvas)->vadjustment->value + canvas->zoom_yofs,
	                     /*canvas->zoom_xofs,
	                        canvas->zoom_yofs,*/
	                     &wxofs, &wyofs);

	canvas->scroll_x1 = x1;
	canvas->scroll_y1 = y1;
	canvas->scroll_x2 = x2;
	canvas->scroll_y2 = y2;

	ganv_canvas_base_w2c(canvas, wxofs, wyofs, &xofs, &yofs);

	scroll_to(canvas, xofs, yofs);

	canvas->need_repick = TRUE;
#if 0
	/* todo: should be requesting update */
	(*GANV_ITEM_CLASS(canvas->root->object.klass)->update)(
	    canvas->root, NULL, NULL, 0);
#endif
}

/**
 * ganv_canvas_base_get_scroll_region:
 * @canvas: A canvas.
 * @x1: Leftmost limit of the scrolling region (return value).
 * @y1: Upper limit of the scrolling region (return value).
 * @x2: Rightmost limit of the scrolling region (return value).
 * @y2: Lower limit of the scrolling region (return value).
 *
 * Queries the scrolling region of a canvas.
 **/
void
ganv_canvas_base_get_scroll_region(GanvCanvasBase* canvas, double* x1, double* y1, double* x2,
                                   double* y2)
{
	g_return_if_fail(GANV_IS_CANVAS_BASE(canvas));

	if (x1) {
		*x1 = canvas->scroll_x1;
	}

	if (y1) {
		*y1 = canvas->scroll_y1;
	}

	if (x2) {
		*x2 = canvas->scroll_x2;
	}

	if (y2) {
		*y2 = canvas->scroll_y2;
	}
}

/**
 * ganv_canvas_base_set_center_scroll_region:
 * @canvas: A canvas.
 * @center_scroll_region: Whether to center the scrolling region in the canvas
 * window when it is smaller than the canvas' allocation.
 *
 * When the scrolling region of the canvas is smaller than the canvas window,
 * e.g.  the allocation of the canvas, it can be either centered on the window
 * or simply made to be on the upper-left corner on the window.  This function
 * lets you configure this property.
 **/
void
ganv_canvas_base_set_center_scroll_region(GanvCanvasBase* canvas, gboolean center_scroll_region)
{
	g_return_if_fail(GANV_IS_CANVAS_BASE(canvas));

	canvas->center_scroll_region = center_scroll_region != 0;

	scroll_to(canvas,
	          canvas->layout.hadjustment->value,
	          canvas->layout.vadjustment->value);
}

/**
 * ganv_canvas_base_get_center_scroll_region:
 * @canvas: A canvas.
 *
 * Returns whether the canvas is set to center the scrolling region in the window
 * if the former is smaller than the canvas' allocation.
 *
 * Return value: Whether the scroll region is being centered in the canvas window.
 **/
gboolean
ganv_canvas_base_get_center_scroll_region(GanvCanvasBase* canvas)
{
	g_return_val_if_fail(GANV_IS_CANVAS_BASE(canvas), FALSE);

	return canvas->center_scroll_region ? TRUE : FALSE;
}

/**
 * ganv_canvas_base_set_pixels_per_unit:
 * @canvas: A canvas.
 * @n: The number of pixels that correspond to one canvas unit.
 *
 * Sets the zooming factor of a canvas by specifying the number of pixels that
 * correspond to one canvas unit.
 *
 * The anchor point for zooming, i.e. the point that stays fixed and all others
 * zoom inwards or outwards from it, depends on whether the canvas is set to
 * center the scrolling region or not.  You can control this using the
 * ganv_canvas_base_set_center_scroll_region() function.  If the canvas is set to
 * center the scroll region, then the center of the canvas window is used as the
 * anchor point for zooming.  Otherwise, the upper-left corner of the canvas
 * window is used as the anchor point.
 **/
void
ganv_canvas_base_set_pixels_per_unit(GanvCanvasBase* canvas, double n)
{
	double ax, ay;
	int    x1, y1;
	int    anchor_x, anchor_y;

	g_return_if_fail(GANV_IS_CANVAS_BASE(canvas));
	g_return_if_fail(n > GANV_CANVAS_BASE_EPSILON);

	if (canvas->center_scroll_region) {
		anchor_x = GTK_WIDGET(canvas)->allocation.width / 2;
		anchor_y = GTK_WIDGET(canvas)->allocation.height / 2;
	} else {
		anchor_x = anchor_y = 0;
	}

	/* Find the coordinates of the anchor point in units. */
	if (canvas->layout.hadjustment) {
		ax
		    = (canvas->layout.hadjustment->value
		       + anchor_x) / canvas->pixels_per_unit + canvas->scroll_x1 + canvas->zoom_xofs;
	} else {
		ax = (0.0 + anchor_x) / canvas->pixels_per_unit + canvas->scroll_x1 + canvas->zoom_xofs;
	}
	if (canvas->layout.hadjustment) {
		ay
		    = (canvas->layout.vadjustment->value
		       + anchor_y) / canvas->pixels_per_unit + canvas->scroll_y1 + canvas->zoom_yofs;
	} else {
		ay = (0.0 + anchor_y) / canvas->pixels_per_unit + canvas->scroll_y1 + canvas->zoom_yofs;
	}

	/* Now calculate the new offset of the upper left corner. */
	x1 = ((ax - canvas->scroll_x1) * n) - anchor_x;
	y1 = ((ay - canvas->scroll_y1) * n) - anchor_y;

	canvas->pixels_per_unit = n;

	scroll_to(canvas, x1, y1);

	ganv_canvas_base_request_update(canvas);
	canvas->need_repick = TRUE;
}

/**
 * ganv_canvas_base_scroll_to:
 * @canvas: A canvas.
 * @cx: Horizontal scrolling offset in canvas pixel units.
 * @cy: Vertical scrolling offset in canvas pixel units.
 *
 * Makes a canvas scroll to the specified offsets, given in canvas pixel units.
 * The canvas will adjust the view so that it is not outside the scrolling
 * region.  This function is typically not used, as it is better to hook
 * scrollbars to the canvas layout's scrolling adjusments.
 **/
void
ganv_canvas_base_scroll_to(GanvCanvasBase* canvas, int cx, int cy)
{
	g_return_if_fail(GANV_IS_CANVAS_BASE(canvas));

	scroll_to(canvas, cx, cy);
}

/**
 * ganv_canvas_base_get_scroll_offsets:
 * @canvas: A canvas.
 * @cx: Horizontal scrolling offset (return value).
 * @cy: Vertical scrolling offset (return value).
 *
 * Queries the scrolling offsets of a canvas.  The values are returned in canvas
 * pixel units.
 **/
void
ganv_canvas_base_get_scroll_offsets(GanvCanvasBase* canvas, int* cx, int* cy)
{
	g_return_if_fail(GANV_IS_CANVAS_BASE(canvas));

	if (cx) {
		*cx = canvas->layout.hadjustment->value;
	}

	if (cy) {
		*cy = canvas->layout.vadjustment->value;
	}
}

/**
 * ganv_canvas_base_update_now:
 * @canvas: A canvas.
 *
 * Forces an immediate update and redraw of a canvas.  If the canvas does not
 * have any pending update or redraw requests, then no action is taken.  This is
 * typically only used by applications that need explicit control of when the
 * display is updated, like games.  It is not needed by normal applications.
 */
void
ganv_canvas_base_update_now(GanvCanvasBase* canvas)
{
	g_return_if_fail(GANV_IS_CANVAS_BASE(canvas));

	if (!(canvas->need_update || canvas->need_redraw)) {
		g_assert(canvas->idle_id == 0);
		g_assert(canvas->redraw_region == NULL);
		return;
	}

	remove_idle(canvas);
	do_update(canvas);
}

/**
 * ganv_canvas_base_get_item_at:
 * @canvas: A canvas.
 * @x: X position in world coordinates.
 * @y: Y position in world coordinates.
 *
 * Looks for the item that is under the specified position, which must be
 * specified in world coordinates.
 *
 * Return value: (transfer none): The sought item, or NULL if no item is at the
 * specified coordinates.
 **/
GanvItem*
ganv_canvas_base_get_item_at(GanvCanvasBase* canvas, double x, double y)
{
	GanvItem* item;
	double    dist;
	int       cx, cy;

	g_return_val_if_fail(GANV_IS_CANVAS_BASE(canvas), NULL);

	ganv_canvas_base_w2c(canvas, x, y, &cx, &cy);

	dist = GANV_ITEM_GET_CLASS(canvas->root)->point(
		canvas->root,
		x - canvas->root->x, y - canvas->root->y,
		cx, cy,
		&item);
	if ((int)(dist * canvas->pixels_per_unit + 0.5) <= canvas->close_enough) {
		return item;
	} else {
		return NULL;
	}
}

/* Queues an update of the canvas */
static void
ganv_canvas_base_request_update(GanvCanvasBase* canvas)
{
	GANV_CANVAS_BASE_GET_CLASS(canvas)->request_update(canvas);
}

static void
ganv_canvas_base_request_update_real(GanvCanvasBase* canvas)
{
	if (canvas->need_update) {
		return;
	}

	canvas->need_update = TRUE;
	if (GTK_WIDGET_MAPPED((GtkWidget*)canvas)) {
		add_idle(canvas);
	}
}

static inline gboolean
rect_overlaps(const IRect* a, const IRect* b)
{
	if ((a->x             > b->x + b->width) ||
	    (a->y             > b->y + b->height) ||
	    (a->x + a->width  < b->x) ||
	    (a->y + a->height < b->y)) {
		return FALSE;
	}
	return TRUE;
}

static inline gboolean
rect_is_visible(GanvCanvasBase* canvas, const IRect* r)
{
	const IRect rect = {
		canvas->layout.hadjustment->value - canvas->zoom_xofs,
		canvas->layout.vadjustment->value - canvas->zoom_yofs,
		GTK_WIDGET(canvas)->allocation.width,
		GTK_WIDGET(canvas)->allocation.height
	};

	return rect_overlaps(&rect, r);
}

/**
 * ganv_canvas_base_request_redraw:
 * @canvas: A canvas.
 * @x1: Leftmost coordinate of the rectangle to be redrawn.
 * @y1: Upper coordinate of the rectangle to be redrawn.
 * @x2: Rightmost coordinate of the rectangle to be redrawn, plus 1.
 * @y2: Lower coordinate of the rectangle to be redrawn, plus 1.
 *
 * Informs a canvas that the specified area needs to be repainted.  To be used
 * only by item implementations.
 **/
void
ganv_canvas_base_request_redraw(GanvCanvasBase* canvas, int x1, int y1, int x2, int y2)
{
	g_return_if_fail(GANV_IS_CANVAS_BASE(canvas));

	if (!GTK_WIDGET_DRAWABLE(canvas) || (x1 >= x2) || (y1 >= y2)) {
		return;
	}

	const IRect rect = { x1, y1, x2 - x1, y2 - y1 };

	if (!rect_is_visible(canvas, &rect)) {
		return;
	}

	IRect* r = (IRect*)g_malloc(sizeof(IRect));
	*r = rect;

	canvas->redraw_region = g_slist_prepend(canvas->redraw_region, r);
	canvas->need_redraw   = TRUE;

	if (canvas->idle_id == 0) {
		add_idle(canvas);
	}
}

/**
 * ganv_canvas_base_w2c_affine:
 * @canvas: A canvas.
 * @matrix: An affine transformation matrix (return value).
 *
 * Gets the affine transform that converts from world coordinates to canvas
 * pixel coordinates.
 **/
void
ganv_canvas_base_w2c_affine(GanvCanvasBase* canvas, cairo_matrix_t* matrix)
{
	g_return_if_fail(GANV_IS_CANVAS_BASE(canvas));
	g_return_if_fail(matrix != NULL);

	cairo_matrix_init_translate(matrix,
	                            -canvas->scroll_x1,
	                            -canvas->scroll_y1);

	cairo_matrix_scale(matrix,
	                   canvas->pixels_per_unit,
	                   canvas->pixels_per_unit);
}

/**
 * ganv_canvas_base_w2c:
 * @canvas: A canvas.
 * @wx: World X coordinate.
 * @wy: World Y coordinate.
 * @cx: X pixel coordinate (return value).
 * @cy: Y pixel coordinate (return value).
 *
 * Converts world coordinates into canvas pixel coordinates.
 **/
void
ganv_canvas_base_w2c(GanvCanvasBase* canvas, double wx, double wy, int* cx, int* cy)
{
	g_return_if_fail(GANV_IS_CANVAS_BASE(canvas));

	cairo_matrix_t matrix;
	ganv_canvas_base_w2c_affine(canvas, &matrix);

	cairo_matrix_transform_point(&matrix, &wx, &wy);

	if (cx) {
		*cx = floor(wx + 0.5);
	}
	if (cy) {
		*cy = floor(wy + 0.5);
	}
}

/**
 * ganv_canvas_base_w2c_d:
 * @canvas: A canvas.
 * @wx: World X coordinate.
 * @wy: World Y coordinate.
 * @cx: X pixel coordinate (return value).
 * @cy: Y pixel coordinate (return value).
 *
 * Converts world coordinates into canvas pixel coordinates.  This
 * version returns coordinates in floating point coordinates, for
 * greater precision.
 **/
void
ganv_canvas_base_w2c_d(GanvCanvasBase* canvas, double wx, double wy, double* cx, double* cy)
{
	g_return_if_fail(GANV_IS_CANVAS_BASE(canvas));

	cairo_matrix_t matrix;
	ganv_canvas_base_w2c_affine(canvas, &matrix);

	cairo_matrix_transform_point(&matrix, &wx, &wy);

	if (cx) {
		*cx = wx;
	}
	if (cy) {
		*cy = wy;
	}
}

/**
 * ganv_canvas_base_c2w:
 * @canvas: A canvas.
 * @cx: Canvas pixel X coordinate.
 * @cy: Canvas pixel Y coordinate.
 * @wx: X world coordinate (return value).
 * @wy: Y world coordinate (return value).
 *
 * Converts canvas pixel coordinates to world coordinates.
 **/
void
ganv_canvas_base_c2w(GanvCanvasBase* canvas, int cx, int cy, double* wx, double* wy)
{
	g_return_if_fail(GANV_IS_CANVAS_BASE(canvas));

	cairo_matrix_t matrix;
	ganv_canvas_base_w2c_affine(canvas, &matrix);
	cairo_matrix_invert(&matrix);

	double x = cx;
	double y = cy;
	cairo_matrix_transform_point(&matrix, &x, &y);

	if (wx) {
		*wx = x;
	}
	if (wy) {
		*wy = y;
	}
}

/**
 * ganv_canvas_base_window_to_world:
 * @canvas: A canvas.
 * @winx: Window-relative X coordinate.
 * @winy: Window-relative Y coordinate.
 * @worldx: X world coordinate (return value).
 * @worldy: Y world coordinate (return value).
 *
 * Converts window-relative coordinates into world coordinates.  You can use
 * this when you need to convert mouse coordinates into world coordinates, for
 * example.
 **/
void
ganv_canvas_base_window_to_world(GanvCanvasBase* canvas, double winx, double winy,
                                 double* worldx, double* worldy)
{
	g_return_if_fail(GANV_IS_CANVAS_BASE(canvas));

	if (worldx) {
		*worldx = canvas->scroll_x1 + ((winx - canvas->zoom_xofs)
		                               / canvas->pixels_per_unit);
	}

	if (worldy) {
		*worldy = canvas->scroll_y1 + ((winy - canvas->zoom_yofs)
		                               / canvas->pixels_per_unit);
	}
}

/**
 * ganv_canvas_base_world_to_window:
 * @canvas: A canvas.
 * @worldx: World X coordinate.
 * @worldy: World Y coordinate.
 * @winx: X window-relative coordinate.
 * @winy: Y window-relative coordinate.
 *
 * Converts world coordinates into window-relative coordinates.
 **/
void
ganv_canvas_base_world_to_window(GanvCanvasBase* canvas, double worldx, double worldy,
                                 double* winx, double* winy)
{
	g_return_if_fail(GANV_IS_CANVAS_BASE(canvas));

	if (winx) {
		*winx = (canvas->pixels_per_unit) * (worldx - canvas->scroll_x1) + canvas->zoom_xofs;
	}

	if (winy) {
		*winy = (canvas->pixels_per_unit) * (worldy - canvas->scroll_y1) + canvas->zoom_yofs;
	}
}

static gboolean
boolean_handled_accumulator(GSignalInvocationHint* ihint,
                            GValue*                return_accu,
                            const GValue*          handler_return,
                            gpointer               dummy)
{
	gboolean continue_emission;
	gboolean signal_handled;

	signal_handled = g_value_get_boolean(handler_return);
	g_value_set_boolean(return_accu, signal_handled);
	continue_emission = !signal_handled;

	return continue_emission;
}

/* Class initialization function for GanvItemClass */
static void
ganv_item_class_init(GanvItemClass* klass)
{
	GObjectClass* gobject_class;

	gobject_class = (GObjectClass*)klass;

	item_parent_class = (GtkObjectClass*)g_type_class_peek_parent(klass);

	gobject_class->set_property = ganv_item_set_property;
	gobject_class->get_property = ganv_item_get_property;

	g_object_class_install_property
	    (gobject_class, ITEM_PROP_PARENT,
	    g_param_spec_object("parent", NULL, NULL,
	                        GANV_TYPE_ITEM,
	                        (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	g_object_class_install_property
	    (gobject_class, ITEM_PROP_X,
	    g_param_spec_double("x",
	                        _("X"),
	                        _("X"),
	                        -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
	                        (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property
	    (gobject_class, ITEM_PROP_Y,
	    g_param_spec_double("y",
	                        _("Y"),
	                        _("Y"),
	                        -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
	                        (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	g_object_class_install_property
	    (gobject_class, ITEM_PROP_MANAGED,
	    g_param_spec_boolean("managed",
	                        _("Managed"),
	                        _("Whether the item is managed by its parent"),
	                         0,
	                        (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	item_signals[ITEM_EVENT]
	    = g_signal_new("event",
	                   G_TYPE_FROM_CLASS(klass),
	                   G_SIGNAL_RUN_LAST,
	                   G_STRUCT_OFFSET(GanvItemClass, event),
	                   boolean_handled_accumulator, NULL,
	                   ganv_marshal_BOOLEAN__BOXED,
	                   G_TYPE_BOOLEAN, 1,
	                   GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

	gobject_class->dispose = ganv_item_dispose;

	klass->realize   = ganv_item_realize;
	klass->unrealize = ganv_item_unrealize;
	klass->map       = ganv_item_map;
	klass->unmap     = ganv_item_unmap;
	klass->update    = ganv_item_update;
	klass->point     = ganv_item_point;
}
