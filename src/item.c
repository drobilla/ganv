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

#include "ganv/canvas.h"
#include "ganv/canvas-base.h"
#include "ganv/node.h"

#include "./boilerplate.h"
#include "./ganv-marshal.h"
#include "./ganv-private.h"
#include "./gettext.h"

#define GCI_UPDATE_MASK (GANV_CANVAS_BASE_UPDATE_REQUESTED \
                         | GANV_CANVAS_BASE_UPDATE_AFFINE \
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

static void
ganv_item_set_property(GObject*      object,
                       guint         prop_id,
                       const GValue* value,
                       GParamSpec*   pspec)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(GANV_IS_ITEM(object));

	GanvItem* item = GANV_ITEM(object);

	switch (prop_id) {
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
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
ganv_item_get_property(GObject*    object,
                       guint       prop_id,
                       GValue*     value,
                       GParamSpec* pspec)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(GANV_IS_ITEM(object));

	GanvItem* item = GANV_ITEM(object);

	switch (prop_id) {
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
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
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
	if (!item || !GANV_IS_ITEM(item)) {
		return;
	}

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

void
ganv_item_i2w_offset(GanvItem* item, double* px, double* py)
{
	double x = 0.0;
	double y = 0.0;
  	while (item) {
		x += item->x;
		y += item->y;
		item = item->parent;
	}
	*px = x;
	*py = y;
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
	/*g_return_if_fail(GANV_IS_ITEM(item));
	g_return_if_fail(x != NULL);
	g_return_if_fail(y != NULL);*/

	double off_x;
	double off_y;
	ganv_item_i2w_offset(item, &off_x, &off_y);

	*x += off_x;
	*y += off_y;
}

void
ganv_item_i2w_pair(GanvItem* item, double* x1, double* y1, double* x2, double* y2)
{
	double off_x;
	double off_y;
	ganv_item_i2w_offset(item, &off_x, &off_y);

	*x1 += off_x;
	*y1 += off_y;
	*x2 += off_x;
	*y2 += off_y;
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
	double off_x;
	double off_y;
	ganv_item_i2w_offset(item, &off_x, &off_y);

	*x -= off_x;
	*y -= off_y;
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

		ganv_canvas_base_emit_event(item->canvas, &ev);
	}

	item->canvas->focused_item = item;
	gtk_widget_grab_focus(GTK_WIDGET(item->canvas));

	if (focused_item) {
		ev.focus_change.type       = GDK_FOCUS_CHANGE;
		ev.focus_change.window     = GTK_LAYOUT(item->canvas)->bin_window;
		ev.focus_change.send_event = FALSE;
		ev.focus_change.in         = TRUE;

		ganv_canvas_base_emit_event(item->canvas, &ev);
	}
}

void
ganv_item_emit_event(GanvItem* item, GdkEvent* event, gint* finished)
{
	g_signal_emit(item, item_signals[ITEM_EVENT], 0, event, finished);
}

static void
ganv_item_default_bounds(GanvItem* item, double* x1, double* y1, double* x2, double* y2)
{
	*x1 = *y1 = *x2 = *y2 = 0.0;
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
	GANV_ITEM_GET_CLASS(item)->bounds(item, x1, y1, x2, y2);
}

/**
 * ganv_item_request_update:
 * @item: A canvas item.
 *
 * To be used only by item implementations.  Requests that the canvas queue an
 * update for the specified item.
 **/
void
ganv_item_request_update(GanvItem* item)
{
	/* Note: At some point, if this short-circuit was enabled, the canvas stopped
	   updating items entirely when connected modules are removed. */
	if (item->object.flags & GANV_ITEM_NEED_UPDATE) {
		return;
	}

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
	klass->bounds    = ganv_item_default_bounds;
}
