/* This file is part of Ganv.
 * Copyright 2007-2012 David Robillard <http://drobilla.net>
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

#include "ganv/item.h"

G_BEGIN_DECLS

/* "Small" value used by canvas stuff */
#define GANV_CANVAS_BASE_EPSILON 1e-10

typedef struct _GanvCanvasBase      GanvCanvasBase;
typedef struct _GanvCanvasBaseClass GanvCanvasBaseClass;

/* Update flags for items */
enum {
	GANV_CANVAS_BASE_UPDATE_REQUESTED  = 1 << 0,
	GANV_CANVAS_BASE_UPDATE_AFFINE     = 1 << 1,
	GANV_CANVAS_BASE_UPDATE_VISIBILITY = 1 << 2,
};

#define GANV_TYPE_CANVAS_BASE            (ganv_canvas_base_get_type())
#define GANV_CANVAS_BASE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GANV_TYPE_CANVAS_BASE, GanvCanvasBase))
#define GANV_CANVAS_BASE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GANV_TYPE_CANVAS_BASE, GanvCanvasBaseClass))
#define GANV_IS_CANVAS_BASE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GANV_TYPE_CANVAS_BASE))
#define GANV_IS_CANVAS_BASE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GANV_TYPE_CANVAS_BASE))
#define GANV_CANVAS_BASE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GANV_TYPE_CANVAS_BASE, GanvCanvasBaseClass))

struct _GanvCanvasBase {
	GtkLayout layout;

	/* Root canvas item */
	GanvItem* root;

	/* Region that needs redrawing (list of rectangles) */
	GSList* redraw_region;

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
