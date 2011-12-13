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

/* Based on GnomeCanvas, by Federico Mena <federico@nuclecu.unam.mx>
 * and Raph Levien <raph@gimp.org>
 * Copyright 1997-2000 Free Software Foundation
 */

#ifndef GANV_CANVAS_BASE_H
#define GANV_CANVAS_BASE_H

#include <stdarg.h>

#include <cairo.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS


/* "Small" value used by canvas stuff */
#define GANV_CANVAS_BASE_EPSILON 1e-10

typedef struct _GanvCanvasBase      GanvCanvasBase;
typedef struct _GanvCanvasBaseClass GanvCanvasBaseClass;
typedef struct _GanvItem            GanvItem;
typedef struct _GanvItemClass       GanvItemClass;


/* GanvItem - base item class for canvas items
 *
 * All canvas items are derived from GanvItem.  The only information a
 * GanvItem contains is its parent canvas, its parent canvas item group,
 * its bounding box in world coordinates, and its current affine transformation.
 *
 * Items inside a canvas are organized in a tree of GanvItemGroup nodes
 * and GanvItem leaves.  Each canvas has a single root group, which can
 * be obtained with the ganv_canvas_base_get_root() function.
 *
 * The abstract GanvItem class does not have any configurable or
 * queryable attributes.
 */

/* Object flags for items */
enum {
	GANV_ITEM_REALIZED      = 1 << 1,
	GANV_ITEM_MAPPED        = 1 << 2,
	GANV_ITEM_ALWAYS_REDRAW = 1 << 3,
	GANV_ITEM_VISIBLE       = 1 << 4,
	GANV_ITEM_NEED_UPDATE   = 1 << 5,
	GANV_ITEM_NEED_VIS      = 1 << 6,
};

/* Update flags for items */
enum {
	GANV_CANVAS_BASE_UPDATE_REQUESTED  = 1 << 0,
	GANV_CANVAS_BASE_UPDATE_AFFINE     = 1 << 1,
	GANV_CANVAS_BASE_UPDATE_VISIBILITY = 1 << 2,
};

#define GANV_TYPE_ITEM            (ganv_item_get_type())
#define GANV_ITEM(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GANV_TYPE_ITEM, GanvItem))
#define GANV_ITEM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GANV_TYPE_ITEM, GanvItemClass))
#define GANV_IS_ITEM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GANV_TYPE_ITEM))
#define GANV_IS_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GANV_TYPE_ITEM))
#define GANV_ITEM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GANV_TYPE_ITEM, GanvItemClass))

struct _GanvItem {
	GtkObject object;

	/* Parent canvas for this item */
	GanvCanvasBase* canvas;

	/* Parent for this item */
	GanvItem* parent;

	/* Position in parent-relative coordinates. */
	double x, y;

	/* Bounding box for this item (in canvas coordinates) */
	double x1, y1, x2, y2;
};

struct _GanvItemClass {
	GtkObjectClass parent_class;

	/* Add a child to this item (optional) */
	void (* add)(GanvItem* item, GanvItem* child);

	/* Remove a child from this item (optional) */
	void (* remove)(GanvItem* item, GanvItem* child);

	/* Tell the item to update itself.  The flags are from the update flags
	 * defined above.  The item should update its internal state from its
	 * queued state, and recompute and request its repaint area.  The update
	 * method also recomputes the bounding box of the item.
	 */
	void (* update)(GanvItem* item, int flags);

	/* Realize an item -- create GCs, etc. */
	void (* realize)(GanvItem* item);

	/* Unrealize an item */
	void (* unrealize)(GanvItem* item);

	/* Map an item - normally only need by items with their own GdkWindows */
	void (* map)(GanvItem* item);

	/* Unmap an item */
	void (* unmap)(GanvItem* item);

	/* Draw an item of this type.  (x, y) are the upper-left canvas pixel
	 * coordinates of the drawable, a temporary pixmap, where things get
	 * drawn.  (width, height) are the dimensions of the drawable.
	 */
	void (* draw)(GanvItem* item, cairo_t* cr,
	              int x, int y, int width, int height);

	/* Calculate the distance from an item to the specified point.  It also
	 * returns a canvas item which is actual item the point is within, which
	 * may not be equal to @item if @item has children.  (cx, cy) are the
	 * canvas pixel coordinates that correspond to the item-relative
	 * coordinates (x, y).
	 */
	double (* point)(GanvItem* item, double x, double y, int cx, int cy,
	                 GanvItem** actual_item);

	/* Fetch the item's bounding box (need not be exactly tight).  This
	 * should be in item-relative coordinates.
	 */
	void (* bounds)(GanvItem* item, double* x1, double* y1, double* x2, double* y2);

	/* Signal: an event occurred for an item of this type.  The (x, y)
	 * coordinates are in the canvas world coordinate system.
	 */
	gboolean (* event)(GanvItem* item, GdkEvent* event);

	/* Reserved for future expansion */
	gpointer spare_vmethods [4];
};


GType ganv_item_get_type(void) G_GNUC_CONST;

/* Create a canvas item using the standard Gtk argument mechanism.  The item
 * automatically added to @parent.  The last argument must be a NULL pointer.
 */
GanvItem* ganv_item_new(GanvItem* parent, GType type,
                        const gchar* first_arg_name, ...);

/* Constructors for use in derived classes and language wrappers */
void ganv_item_construct(GanvItem* item, GanvItem* parent,
                         const gchar* first_arg_name, va_list args);

/* Configure an item using the standard Gtk argument mechanism.  The last
 * argument must be a NULL pointer.
 */
void ganv_item_set(GanvItem* item, const gchar* first_arg_name, ...);

/* Used only for language wrappers and the like */
void ganv_item_set_valist(GanvItem* item,
                          const gchar* first_arg_name, va_list args);

/* Move an item by the specified amount */
void ganv_item_move(GanvItem* item, double dx, double dy);

/* Raise an item to the top of its parent's z-order. */
void ganv_item_raise_to_top(GanvItem* item);

/* Lower an item to the bottom of its parent's z-order */
void ganv_item_lower_to_bottom(GanvItem* item);

/* Show an item (make it visible).  If the item is already shown, it has no
 * effect.
 */
void ganv_item_show(GanvItem* item);

/* Hide an item (make it invisible).  If the item is already invisible, it has
 * no effect.
 */
void ganv_item_hide(GanvItem* item);

/* Grab the mouse for the specified item.  Only the events in event_mask will be
 * reported.  If cursor is non-NULL, it will be used during the duration of the
 * grab.  Time is a proper X event time parameter.  Returns the same values as
 * XGrabPointer().
 */
int ganv_item_grab(GanvItem* item, unsigned int event_mask,
                   GdkCursor* cursor, guint32 etime);

/* Ungrabs the mouse -- the specified item must be the same that was passed to
 * ganv_item_grab().  Time is a proper X event time parameter.
 */
void ganv_item_ungrab(GanvItem* item, guint32 etime);

/* These functions convert from a coordinate system to another.  "w" is world
 * coordinates and "i" is item coordinates.
 */
void ganv_item_w2i(GanvItem* item, double* x, double* y);
void ganv_item_i2w(GanvItem* item, double* x, double* y);

/* Gets the affine transform that converts from item-relative coordinates to
 * world coordinates.
 */
void ganv_item_i2w_affine(GanvItem* item, cairo_matrix_t* matrix);

/* Gets the affine transform that converts from item-relative coordinates to
 * canvas pixel coordinates.
 */
void ganv_item_i2c_affine(GanvItem* item, cairo_matrix_t* matrix);

/* Used to send all of the keystroke events to a specific item as well as
 * GDK_FOCUS_CHANGE events.
 */
void ganv_item_grab_focus(GanvItem* item);

/* Fetch the bounding box of the item.  The bounding box may not be exactly
 * tight, but the canvas items will do the best they can.  The returned bounding
 * box is in the coordinate system of the item's parent.
 */
void ganv_item_get_bounds(GanvItem* item,
                          double* x1, double* y1, double* x2, double* y2);

/* Request that the update method eventually get called.  This should be used
 * only by item implementations.
 */
void ganv_item_request_update(GanvItem* item);


/*** GanvCanvasBase ***/


#define GANV_TYPE_CANVAS_BASE            (ganv_canvas_base_get_type())
#define GANV_CANVAS_BASE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GANV_TYPE_CANVAS_BASE, GanvCanvasBase))
#define GANV_CANVAS_BASE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GANV_TYPE_CANVAS_BASE, GanvCanvasBaseClass))
#define GANV_IS_CANVAS_BASE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GANV_TYPE_CANVAS_BASE))
#define GANV_IS_CANVAS_BASE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GANV_TYPE_CANVAS_BASE))
#define GANV_CANVAS_BASE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GANV_TYPE_CANVAS_BASE, GanvCanvasBaseClass))


struct _GanvCanvasBase {
	GtkLayout layout;

	/* Root canvas group */
	GanvItem* root;

	/* Region that needs redrawing, stored as a microtile array */
	cairo_region_t* redraw_region;

	/* The item containing the mouse pointer, or NULL if none */
	GanvItem* current_item;

	/* Item that is about to become current (used to track deletions and such) */
	GanvItem* new_current_item;

	/* Item that holds a pointer grab, or NULL if none */
	GanvItem* grabbed_item;

	/* If non-NULL, the currently focused item */
	GanvItem* focused_item;

	/* GC for temporary draw pixmap */
	GdkGC* pixmap_gc;

	/* Event on which selection of current item is based */
	GdkEvent pick_event;

	/* Scrolling region */
	double scroll_x1, scroll_y1;
	double scroll_x2, scroll_y2;

	/* Scaling factor to be used for display */
	double pixels_per_unit;

	/* Idle handler ID */
	guint idle_id;

	/* Signal handler ID for destruction of the root item */
	guint root_destroy_id;

	/* Area that is being redrawn.  Contains (x1, y1) but not (x2, y2).
	 * Specified in canvas pixel coordinates.
	 */
	int redraw_x1, redraw_y1;
	int redraw_x2, redraw_y2;

	/* Offsets of the temprary drawing pixmap */
	int draw_xofs, draw_yofs;

	/* Internal pixel offsets when zoomed out */
	int zoom_xofs, zoom_yofs;

	/* Last known modifier state, for deferred repick when a button is down */
	int state;

	/* Event mask specified when grabbing an item */
	guint grabbed_event_mask;

	/* Tolerance distance for picking items */
	int close_enough;

	/* Whether the canvas should center the scroll region in the middle of
	 * the window if the scroll region is smaller than the window.
	 */
	unsigned int center_scroll_region : 1;

	/* Whether items need update at next idle loop iteration */
	unsigned int need_update : 1;

	/* Whether the canvas needs redrawing at the next idle loop iteration */
	unsigned int need_redraw : 1;

	/* Whether current item will be repicked at next idle loop iteration */
	unsigned int need_repick : 1;

	/* For use by internal pick_current_item() function */
	unsigned int left_grabbed_item : 1;

	/* For use by internal pick_current_item() function */
	unsigned int in_repick : 1;
};

struct _GanvCanvasBaseClass {
	GtkLayoutClass parent_class;

	/* Draw the background for the area given. This method is only used
	 * for non-antialiased canvases.
	 */
	void (* draw_background)(GanvCanvasBase* canvas, GdkDrawable* drawable,
	                         int x, int y, int width, int height);

	/* Private Virtual methods for groping the canvas inside bonobo */
	void (* request_update)(GanvCanvasBase* canvas);

	/* Reserved for future expansion */
	gpointer spare_vmethods [4];
};


GType ganv_canvas_base_get_type(void) G_GNUC_CONST;

/* Creates a new canvas.  You should check that the canvas is created with the
 * proper visual and colormap.  Any visual will do unless you intend to insert
 * gdk_imlib images into it, in which case you should use the gdk_imlib visual.
 *
 * You should call ganv_canvas_base_set_scroll_region() soon after calling this
 * function to set the desired scrolling limits for the canvas.
 */
GtkWidget* ganv_canvas_base_new(void);

/* Returns the root item of the canvas */
GanvItem* ganv_canvas_base_root(GanvCanvasBase* canvas);

/* Sets the limits of the scrolling region, in world coordinates */
void ganv_canvas_base_set_scroll_region(GanvCanvasBase* canvas,
                                        double x1, double y1, double x2, double y2);

/* Gets the limits of the scrolling region, in world coordinates */
void ganv_canvas_base_get_scroll_region(GanvCanvasBase* canvas,
                                        double* x1, double* y1, double* x2, double* y2);

/* Whether the canvas centers the scroll region if it is smaller than the window */
void ganv_canvas_base_set_center_scroll_region(GanvCanvasBase* canvas, gboolean center_scroll_region);

/* Returns whether the canvas is set to center the scroll region if it is smaller than the window */
gboolean ganv_canvas_base_get_center_scroll_region(GanvCanvasBase* canvas);

/* Sets the number of pixels that correspond to one unit in world coordinates */
void ganv_canvas_base_set_pixels_per_unit(GanvCanvasBase* canvas, double n);

/* Scrolls the canvas to the specified offsets, given in canvas pixel coordinates */
void ganv_canvas_base_scroll_to(GanvCanvasBase* canvas, int cx, int cy);

/* Returns the scroll offsets of the canvas in canvas pixel coordinates.  You
 * can specify NULL for any of the values, in which case that value will not be
 * queried.
 */
void ganv_canvas_base_get_scroll_offsets(GanvCanvasBase* canvas, int* cx, int* cy);

/* Requests that the canvas be repainted immediately instead of in the idle
 * loop.
 */
void ganv_canvas_base_update_now(GanvCanvasBase* canvas);

/* Returns the item that is at the specified position in world coordinates, or
 * NULL if no item is there.
 */
GanvItem* ganv_canvas_base_get_item_at(GanvCanvasBase* canvas, double x, double y);

/* For use only by item type implementations.  Request that the canvas
 * eventually redraw the specified region, specified in canvas pixel
 * coordinates.  The region contains (x1, y1) but not (x2, y2).
 */
void ganv_canvas_base_request_redraw(GanvCanvasBase* canvas, int x1, int y1, int x2, int y2);

/* Gets the affine transform that converts world coordinates into canvas pixel
 * coordinates.
 */
void ganv_canvas_base_w2c_affine(GanvCanvasBase* canvas, cairo_matrix_t* matrix);

/* These functions convert from a coordinate system to another.  "w" is world
 * coordinates, "c" is canvas pixel coordinates (pixel coordinates that are
 * (0,0) for the upper-left scrolling limit and something else for the
 * lower-left scrolling limit).
 */
void ganv_canvas_base_w2c(GanvCanvasBase* canvas, double wx, double wy, int* cx, int* cy);
void ganv_canvas_base_w2c_d(GanvCanvasBase* canvas, double wx, double wy, double* cx, double* cy);
void ganv_canvas_base_c2w(GanvCanvasBase* canvas, int cx, int cy, double* wx, double* wy);

/* This function takes in coordinates relative to the GTK_LAYOUT
 * (canvas)->bin_window and converts them to world coordinates.
 */
void ganv_canvas_base_window_to_world(GanvCanvasBase* canvas,
                                      double winx, double winy, double* worldx, double* worldy);

/* This is the inverse of ganv_canvas_base_window_to_world() */
void ganv_canvas_base_world_to_window(GanvCanvasBase* canvas,
                                      double worldx, double worldy, double* winx, double* winy);

G_END_DECLS

#endif
