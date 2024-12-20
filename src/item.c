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

/* Based on GnomeCanvas, by Federico Mena <federico@nuclecu.unam.mx>
 * and Raph Levien <raph@gimp.org>
 * Copyright 1997-2000 Free Software Foundation
 */

#include "ganv-marshal.h"
#include "ganv-private.h"
#include "gettext.h"

#include <ganv/item.h>
#include <ganv/types.h>

#include <gdk/gdk.h>
#include <glib-object.h>
#include <glib.h>
#include <gtk/gtk.h>

#include <stdarg.h>
#include <stdio.h>

/* All canvas items are derived from GanvItem.  The only information a GanvItem
 * contains is its parent canvas, its parent canvas item, its bounding box in
 * world coordinates, and its current affine transformation.
 *
 * Items inside a canvas are organized in a tree, where leaves are items
 * without any children.  Each canvas has a single root item, which can be
 * obtained with the ganv_canvas_base_get_root() function.
 *
 * The abstract GanvItem class does not have any configurable or queryable
 * attributes.
 */

/* Update flags for items */
enum {
	GANV_CANVAS_UPDATE_REQUESTED  = 1 << 0,
	GANV_CANVAS_UPDATE_AFFINE     = 1 << 1,
	GANV_CANVAS_UPDATE_VISIBILITY = 1 << 2
};

#define GCI_UPDATE_MASK (GANV_CANVAS_UPDATE_REQUESTED \
                         | GANV_CANVAS_UPDATE_AFFINE \
                         | GANV_CANVAS_UPDATE_VISIBILITY)

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

G_DEFINE_TYPE_WITH_CODE(GanvItem, ganv_item, GTK_TYPE_OBJECT,
                        G_ADD_PRIVATE(GanvItem))

static GtkObjectClass* item_parent_class;

/* Object initialization function for GanvItem */
static void
ganv_item_init(GanvItem* item)
{
	GanvItemPrivate* impl =
	    (GanvItemPrivate*)ganv_item_get_instance_private(item);

	item->object.flags |= GANV_ITEM_VISIBLE;
	item->impl          = impl;
	item->impl->managed = FALSE;
	item->impl->wrapper = NULL;
}

/**
 * ganv_item_new:
 * @parent: The parent group for the new item.
 * @type: The object type of the item.
 * @first_arg_name: A list of object argument name/value pairs, NULL-terminated,
 * used to configure the item.  For example, "fill_color", "black",
 * "width_units", 5.0, NULL.
 * @...: first argument value, second argument name, second argument value, ...
 *
 * Creates a new canvas item with @parent as its parent group.  The item is
 * created at the top of its parent's stack, and starts up as visible.  The item
 * is of the specified @type, for example, it can be
 * ganv_canvas_rect_get_type().  The list of object arguments/value pairs is
 * used to configure the item. If you need to pass construct time parameters, you
 * should use g_object_new() to pass the parameters and
 * ganv_item_construct() to set up the canvas item.
 *
 * Return value: (transfer full): The newly-created item.
 **/
GanvItem*
ganv_item_new(GanvItem* parent, GType type, const gchar* first_arg_name, ...)
{
	g_return_val_if_fail(g_type_is_a(type, ganv_item_get_type()), NULL);

	GanvItem* item = GANV_ITEM(g_object_new(type, NULL));

	va_list args;
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
	GanvItemClass* parent_class = GANV_ITEM_GET_CLASS(item->impl->parent);
	if (!item->impl->managed) {
		if (parent_class->add) {
			parent_class->add(item->impl->parent, item);
		} else {
			g_warning("item added to non-parent item\n");
		}
	}
	ganv_canvas_request_redraw_w(item->impl->canvas,
	                             item->impl->x1, item->impl->y1,
	                             item->impl->x2 + 1, item->impl->y2 + 1);
	ganv_canvas_set_need_repick(item->impl->canvas);
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
		if (item->impl->parent != NULL) {
			g_warning("Cannot set `parent' argument after item has "
			          "already been constructed.");
		} else if (g_value_get_object(value)) {
			item->impl->parent = GANV_ITEM(g_value_get_object(value));
			item->impl->canvas = item->impl->parent->impl->canvas;
			item_post_create_setup(item);
		}
		break;
	case ITEM_PROP_X:
		item->impl->x = g_value_get_double(value);
		ganv_item_request_update(item);
		break;
	case ITEM_PROP_Y:
		item->impl->y = g_value_get_double(value);
		ganv_item_request_update(item);
		break;
	case ITEM_PROP_MANAGED:
		item->impl->managed = g_value_get_boolean(value);
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
		g_value_set_object(value, item->impl->parent);
		break;
	case ITEM_PROP_X:
		g_value_set_double(value, item->impl->x);
		break;
	case ITEM_PROP_Y:
		g_value_set_double(value, item->impl->y);
		break;
	case ITEM_PROP_MANAGED:
		g_value_set_boolean(value, item->impl->managed);
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

	item->impl->parent  = parent;
	item->impl->wrapper = NULL;
	item->impl->canvas  = item->impl->parent->impl->canvas;
	item->impl->layer   = 0;

	g_object_set_valist(G_OBJECT(item), first_arg_name, args);

	item_post_create_setup(item);
}

/* If the item is visible, requests a redraw of it. */
static void
redraw_if_visible(GanvItem* item)
{
	if (item->object.flags & GANV_ITEM_VISIBLE) {
		ganv_canvas_request_redraw_w(item->impl->canvas,
		                             item->impl->x1, item->impl->y1,
		                             item->impl->x2 + 1, item->impl->y2 + 1);
	}
}

/* Standard object dispose function for canvas items */
static void
ganv_item_dispose(GObject* object)
{
	GanvItem* item = NULL;

	g_return_if_fail(GANV_IS_ITEM(object));

	item = GANV_ITEM(object);

	if (item->impl->canvas) {
		redraw_if_visible(item);
		ganv_canvas_forget_item(item->impl->canvas, item);
	}

	/* Normal destroy stuff */

	if (item->object.flags & GANV_ITEM_MAPPED) {
		(*GANV_ITEM_GET_CLASS(item)->unmap)(item);
	}

	if (item->object.flags & GANV_ITEM_REALIZED) {
		(*GANV_ITEM_GET_CLASS(item)->unrealize)(item);
	}

	if (!item->impl->managed && item->impl->parent) {
		if (GANV_ITEM_GET_CLASS(item->impl->parent)->remove) {
			GANV_ITEM_GET_CLASS(item->impl->parent)->remove(item->impl->parent, item);
		} else {
			fprintf(stderr, "warning: Item parent has no remove method\n");
		}
	}

	G_OBJECT_CLASS(item_parent_class)->dispose(object);
	/* items should remove any reference to item->impl->canvas after the
	   first ::destroy */
	item->impl->canvas = NULL;
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
	(void)flags;

	GTK_OBJECT_UNSET_FLAGS(item, GANV_ITEM_NEED_UPDATE);
	GTK_OBJECT_UNSET_FLAGS(item, GANV_ITEM_NEED_VIS);
}

/* Point handler for canvas items */
static double
ganv_item_point(GanvItem* item, double x, double y, GanvItem** actual_item)
{
	(void)item;
	(void)x;
	(void)y;

	*actual_item = NULL;
	return G_MAXDOUBLE;
}

void
ganv_item_invoke_update(GanvItem* item, int flags)
{
	int child_flags = flags;

	/* apply object flags to child flags */

	child_flags &= ~GANV_CANVAS_UPDATE_REQUESTED;

	if (item->object.flags & GANV_ITEM_NEED_UPDATE) {
		child_flags |= GANV_CANVAS_UPDATE_REQUESTED;
	}

	if (item->object.flags & GANV_ITEM_NEED_VIS) {
		child_flags |= GANV_CANVAS_UPDATE_VISIBILITY;
	}

	if (child_flags & GCI_UPDATE_MASK) {
		if (GANV_ITEM_GET_CLASS(item)->update) {
			GANV_ITEM_GET_CLASS(item)->update(item, child_flags);
			g_assert(!(GTK_OBJECT_FLAGS(item) & GANV_ITEM_NEED_UPDATE));
		}
	}
}

/**
 * ganv_item_set:
 * @item: A canvas item.
 * @first_arg_name: The list of object argument name/value pairs used to configure the item.
 * @...: first argument value, second argument name, second argument value, ...
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

	ganv_canvas_set_need_repick(item->impl->canvas);
}

GanvCanvas*
ganv_item_get_canvas(GanvItem* item)
{
	return item->impl->canvas;
}

GanvItem*
ganv_item_get_parent(GanvItem* item)
{
	return item->impl->parent;
}

void
ganv_item_raise(GanvItem* item)
{
	++item->impl->layer;
}

void
ganv_item_lower(GanvItem* item)
{
	--item->impl->layer;
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

	item->impl->x += dx;
	item->impl->y += dy;

	ganv_item_request_update(item);
	ganv_canvas_set_need_repick(item->impl->canvas);
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
		ganv_canvas_request_redraw_w(item->impl->canvas,
		                             item->impl->x1, item->impl->y1,
		                             item->impl->x2 + 1, item->impl->y2 + 1);
		ganv_canvas_set_need_repick(item->impl->canvas);
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
		ganv_canvas_request_redraw_w(item->impl->canvas,
		                             item->impl->x1, item->impl->y1,
		                             item->impl->x2 + 1, item->impl->y2 + 1);
		ganv_canvas_set_need_repick(item->impl->canvas);
	}
}

void
ganv_item_i2w_offset(GanvItem* item, double* px, double* py)
{
	double x = 0.0;
	double y = 0.0;
	while (item) {
		x += item->impl->x;
		y += item->impl->y;
		item = item->impl->parent;
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

	double off_x = 0.0;
	double off_y = 0.0;
	ganv_item_i2w_offset(item, &off_x, &off_y);

	*x += off_x;
	*y += off_y;
}

void
ganv_item_i2w_pair(GanvItem* item, double* x1, double* y1, double* x2, double* y2)
{
	double off_x = 0.0;
	double off_y = 0.0;
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
	double off_x = 0.0;
	double off_y = 0.0;
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
	ganv_canvas_grab_focus(item->impl->canvas, item);
}

void
ganv_item_emit_event(GanvItem* item, GdkEvent* event, gint* finished)
{
	g_signal_emit(item, item_signals[ITEM_EVENT], 0, event, finished);
}

static void
ganv_item_default_bounds(GanvItem* item, double* x1, double* y1, double* x2, double* y2)
{
	(void)item;

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
 * Queries the bounding box of a canvas item.  The bounding box may not be
 * exactly tight, but the canvas items will do the best they can.  The bounds
 * are returned in the coordinate system of the item's parent.
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
	if (!item->impl->canvas) {
		/* Item is being / has been destroyed, ignore */
		return;
	}

	item->object.flags |= GANV_ITEM_NEED_UPDATE;

	if (item->impl->parent != NULL &&
	    !(item->impl->parent->object.flags & GANV_ITEM_NEED_UPDATE)) {
		/* Recurse up the tree */
		ganv_item_request_update(item->impl->parent);
	} else {
		/* Have reached the top of the tree, make sure the update call gets scheduled. */
		ganv_canvas_request_update(item->impl->canvas);
	}
}

void
ganv_item_set_wrapper(GanvItem* item, void* wrapper)
{
	item->impl->wrapper = wrapper;
}

void*
ganv_item_get_wrapper(GanvItem* item)
{
	return item->impl->wrapper;
}

static gboolean
boolean_handled_accumulator(GSignalInvocationHint* ihint,
                            GValue*                return_accu,
                            const GValue*          handler_return,
                            gpointer               dummy)
{
	(void)ihint;
	(void)dummy;

	gboolean continue_emission = FALSE;
	gboolean signal_handled    = FALSE;

	signal_handled = g_value_get_boolean(handler_return);
	g_value_set_boolean(return_accu, signal_handled);
	continue_emission = !signal_handled;

	return continue_emission;
}

/* Class initialization function for GanvItemClass */
static void
ganv_item_class_init(GanvItemClass* klass)
{
	GObjectClass* gobject_class = (GObjectClass*)klass;

	item_parent_class = (GtkObjectClass*)g_type_class_peek_parent(klass);

	gobject_class->set_property = ganv_item_set_property;
	gobject_class->get_property = ganv_item_get_property;

	g_object_class_install_property
		(gobject_class, ITEM_PROP_PARENT,
		 g_param_spec_object("parent", NULL, NULL,
		                     GANV_TYPE_ITEM,
		                     (GParamFlags)(G_PARAM_READABLE | G_PARAM_WRITABLE)));

	g_object_class_install_property
		(gobject_class, ITEM_PROP_X,
		 g_param_spec_double("x",
		                     _("X"),
		                     _("X"),
		                     -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
		                     (GParamFlags)(G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property
		(gobject_class, ITEM_PROP_Y,
		 g_param_spec_double("y",
		                     _("Y"),
		                     _("Y"),
		                     -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
		                     (GParamFlags)(G_PARAM_READABLE | G_PARAM_WRITABLE)));

	g_object_class_install_property
		(gobject_class, ITEM_PROP_MANAGED,
		 g_param_spec_boolean("managed",
		                      _("Managed"),
		                      _("Whether the item is managed by its parent"),
		                      0,
		                      (GParamFlags)(G_PARAM_READABLE | G_PARAM_WRITABLE)));

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
