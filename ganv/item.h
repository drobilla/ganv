/* This file is part of Ganv.
 * Copyright 2007-2014 David Robillard <http://drobilla.net>
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

#ifndef GANV_ITEM_H
#define GANV_ITEM_H

#include <stdarg.h>

#include <cairo.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

struct _GanvCanvas;

typedef struct _GanvItem      GanvItem;
typedef struct _GanvItemImpl  GanvItemImpl;
typedef struct _GanvItemClass GanvItemClass;

/* Object flags for items */
enum {
	GANV_ITEM_REALIZED      = 1 << 1,
	GANV_ITEM_MAPPED        = 1 << 2,
	GANV_ITEM_ALWAYS_REDRAW = 1 << 3,
	GANV_ITEM_VISIBLE       = 1 << 4,
	GANV_ITEM_NEED_UPDATE   = 1 << 5,
	GANV_ITEM_NEED_VIS      = 1 << 6
};

#define GANV_TYPE_ITEM            (ganv_item_get_type())
#define GANV_ITEM(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GANV_TYPE_ITEM, GanvItem))
#define GANV_ITEM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GANV_TYPE_ITEM, GanvItemClass))
#define GANV_IS_ITEM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GANV_TYPE_ITEM))
#define GANV_IS_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GANV_TYPE_ITEM))
#define GANV_ITEM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GANV_TYPE_ITEM, GanvItemClass))

struct _GanvItem {
	GtkObject     object;
	GanvItemImpl* impl;
};

struct _GanvItemClass {
	GtkObjectClass parent_class;

	/* Add a child to this item (optional). */
	void (*add)(GanvItem* item, GanvItem* child);

	/* Remove a child from this item (optional). */
	void (*remove)(GanvItem* item, GanvItem* child);

	/* Tell the item to update itself.
	 *
	 * The flags are from the update flags defined above.  The item should
	 * update its internal state from its queued state, and recompute and
	 * request its repaint area.  The update method also recomputes the
	 * bounding box of the item.
	 */
	void (*update)(GanvItem* item, int flags);

	/* Realize an item (create GCs, etc.). */
	void (*realize)(GanvItem* item);

	/* Unrealize an item. */
	void (*unrealize)(GanvItem* item);

	/* Map an item - normally only need by items with their own GdkWindows. */
	void (*map)(GanvItem* item);

	/* Unmap an item */
	void (*unmap)(GanvItem* item);

	/* Draw an item of this type.
	 *
	 * (cx, cy) and (width, height) describe the rectangle being drawn in
	 * world-relative coordinates.
	 */
	void (*draw)(GanvItem* item,
	             cairo_t*  cr,
	             double    cx,
	             double    cy,
	             double    cw,
	             double    ch);

	/* Calculate the distance from an item to the specified point.
	 *
	 * It also returns a canvas item which is actual item the point is within,
	 * which may not be equal to @item if @item has children.
	 * (x, y) are item-relative coordinates.
	 */
	double (*point)(GanvItem*  item,
	                double     x,
	                double     y,
	                GanvItem** actual_item);

	/* Fetch the item's bounding box (need not be exactly tight).
	 *
	 * This should be in item-relative coordinates.
	 */
	void (*bounds)(GanvItem* item, double* x1, double* y1, double* x2, double* y2);

	/* Signal: an event occurred for an item of this type.
	 *
	 * The (x, y) coordinates are in the canvas world coordinate system.
	 */
	gboolean (*event)(GanvItem* item, GdkEvent* event);

	/* Reserved for future expansion */
	gpointer spare_vmethods[4];
};

GType ganv_item_get_type(void) G_GNUC_CONST;

GanvItem* ganv_item_new(GanvItem* parent, GType type,
                        const gchar* first_arg_name, ...);

void ganv_item_construct(GanvItem* item, GanvItem* parent,
                         const gchar* first_arg_name, va_list args);

void ganv_item_set(GanvItem* item, const gchar* first_arg_name, ...);

void ganv_item_set_valist(GanvItem* item,
                          const gchar* first_arg_name, va_list args);

/**
 * ganv_item_get_canvas:
 * @item: The item.
 *
 * Return value: (transfer none): The canvas @item is on.
 */
struct _GanvCanvas* ganv_item_get_canvas(GanvItem* item);

/**
 * ganv_item_get_parent:
 * @item: The item.
 *
 * Return value: (transfer none): The parent of @item.
 */
GanvItem* ganv_item_get_parent(GanvItem* item);

void ganv_item_raise(GanvItem* item);

void ganv_item_lower(GanvItem* item);

void ganv_item_move(GanvItem* item, double dx, double dy);

void ganv_item_show(GanvItem* item);

void ganv_item_hide(GanvItem* item);

void ganv_item_i2w(GanvItem* item, double* x, double* y);

void ganv_item_w2i(GanvItem* item, double* x, double* y);

void ganv_item_grab_focus(GanvItem* item);

void ganv_item_get_bounds(GanvItem* item,
                          double* x1, double* y1, double* x2, double* y2);

void ganv_item_request_update(GanvItem* item);

void ganv_item_set_wrapper(GanvItem* item, void* wrapper);

void* ganv_item_get_wrapper(GanvItem* item);

G_END_DECLS

#endif  /* GANV_ITEM_H */
