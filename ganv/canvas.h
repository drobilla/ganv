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

#ifndef GANV_CANVAS_H
#define GANV_CANVAS_H

#include "ganv/item.h"
#include "ganv/types.h"

#include <cairo.h>
#include <gdk/gdk.h>
#include <glib-object.h>
#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GANV_TYPE_CANVAS            (ganv_canvas_get_type())
#define GANV_CANVAS(obj)            (GTK_CHECK_CAST((obj), GANV_TYPE_CANVAS, GanvCanvas))
#define GANV_CANVAS_CLASS(klass)    (GTK_CHECK_CLASS_CAST((klass), GANV_TYPE_CANVAS, GanvCanvasClass))
#define GANV_IS_CANVAS(obj)         (GTK_CHECK_TYPE((obj), GANV_TYPE_CANVAS))
#define GANV_IS_CANVAS_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GANV_TYPE_CANVAS))
#define GANV_CANVAS_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS((obj), GANV_TYPE_CANVAS, GanvCanvasClass))

struct _GanvCanvasClass;

typedef struct GanvCanvasImpl   GanvCanvasPrivate;
typedef struct _GanvCanvasClass GanvCanvasClass;

/**
 * GanvDirection:
 * @GANV_DIRECTION_DOWN: Signal flows from top to bottom.
 * @GANV_DIRECTION_RIGHT: Signal flows from left to right.
 *
 * Specifies the direction of signal flow on the canvas, which affects the
 * appearance of modules and how the canvas is auto-arranged.
 */
typedef enum {
	GANV_DIRECTION_DOWN,
	GANV_DIRECTION_RIGHT
} GanvDirection;

struct _GanvCanvas {
	GtkLayout          layout;
	GanvCanvasPrivate* impl;
};

struct _GanvCanvasClass {
	GtkLayoutClass parent_class;

	/* Reserved for future expansion */
	gpointer spare_vmethods[4];
};

GType ganv_canvas_get_type(void) G_GNUC_CONST;

/**
 * GanvEdgeFunc:
 * @edge: Canvas edge.
 * @data: User callback data.
 *
 * A node function that takes a user data argument (for callbacks).
 *
 * Note that in the Gtk world it is considered safe to cast a function to a
 * function with more arguments and call the resulting pointer, so functions
 * like ganv_edge_select can safely be used where a GanvEdgeFunc is expected.
 */
typedef void (*GanvEdgeFunc)(GanvEdge* edge, void* data);

/**
 * GanvNodeFunc:
 * @node: Canvas node.
 * @data: User callback data.
 *
 * A node function that takes a user data argument (for callbacks).
 *
 * Note that in the Gtk world it is considered safe to cast a function to a
 * function with more arguments and call the resulting pointer, so functions
 * like ganv_node_select can safely be used where a GanvNodeFunc is expected.
 */
typedef void (*GanvNodeFunc)(GanvNode* node, void* data);

typedef int (*GanvPortOrderFunc)(const GanvPort*, const GanvPort*, void* data);

/**
 * ganv_canvas_new:
 *
 * Return value: A newly-created canvas.
 */
GanvCanvas*
ganv_canvas_new(double width, double height);

/**
 * ganv_canvas_set_wrapper:
 *
 * Set the opaque wrapper object for the canvas.
 */
void
ganv_canvas_set_wrapper(GanvCanvas* canvas, void* wrapper);

/**
 * ganv_canvas_get_wrapper:
 *
 * Return an opaque pointer to the wrapper for the canvas.
 */
void*
ganv_canvas_get_wrapper(GanvCanvas* canvas);

/**
 * ganv_canvas_clear:
 *
 * Remove all items from the canvas.
 */
void
ganv_canvas_clear(GanvCanvas* canvas);

/**
 * ganv_canvas_empty:
 *
 * Return value: True if there are no items on the canvas.
 */
gboolean
ganv_canvas_empty(const GanvCanvas* canvas);

/**
 * ganv_canvas_get_size:
 *
 * Gets the width and height of the canvas.
 */
void
ganv_canvas_get_size(GanvCanvas* canvas, double* width, double* height);

/**
 * ganv_canvas_resize:
 *
 * Resize the canvas to the given dimensions.
 */
void
ganv_canvas_resize(GanvCanvas* canvas, double width, double height);

/**
 * ganv_canvas_root:
 * @canvas: A canvas.
 *
 * Return value: (transfer none): The root group of the canvas.
 */
GanvItem*
ganv_canvas_root(GanvCanvas* canvas);

/**
 * ganv_canvas_set_scroll_region:
 * @canvas: A canvas.
 * @x1: Leftmost limit of the scrolling region.
 * @y1: Upper limit of the scrolling region.
 * @x2: Rightmost limit of the scrolling region.
 * @y2: Lower limit of the scrolling region.
 *
 * Sets the scrolling region of a canvas to the specified rectangle.  The
 * canvas will then be able to scroll only within this region.  The view of the
 * canvas is adjusted as appropriate to display as much of the new region as
 * possible.
 */
void
ganv_canvas_set_scroll_region(GanvCanvas* canvas,
                              double x1, double y1, double x2, double y2);

/**
 * ganv_canvas_get_scroll_region:
 * @canvas: A canvas.
 * @x1: Leftmost limit of the scrolling region (return value).
 * @y1: Upper limit of the scrolling region (return value).
 * @x2: Rightmost limit of the scrolling region (return value).
 * @y2: Lower limit of the scrolling region (return value).
 *
 * Queries the scrolling region of a canvas.
 */
void
ganv_canvas_get_scroll_region(GanvCanvas* canvas,
                              double* x1, double* y1, double* x2, double* y2);

/**
 * ganv_canvas_set_center_scroll_region:
 * @canvas: A canvas.
 * @center_scroll_region: Whether to center the scrolling region in the canvas
 * window when it is smaller than the canvas' allocation.
 *
 * When the scrolling region of the canvas is smaller than the canvas window,
 * e.g.  the allocation of the canvas, it can be either centered on the window
 * or simply made to be on the upper-left corner on the window.  This function
 * lets you configure this property.
 */
void
ganv_canvas_set_center_scroll_region(GanvCanvas* canvas,
                                     gboolean    center_scroll_region);

/**
 * ganv_canvas_get_center_scroll_region:
 * @canvas: A canvas.
 *
 * Returns whether the canvas is set to center the scrolling region in the
 * window if the former is smaller than the canvas' allocation.
 *
 * Returns: Whether the scroll region is being centered in the canvas window.
 */
gboolean
ganv_canvas_get_center_scroll_region(const GanvCanvas* canvas);

/**
 * ganv_canvas_scroll_to:
 * @canvas: A canvas.
 * @cx: Horizontal scrolling offset in canvas pixel units.
 * @cy: Vertical scrolling offset in canvas pixel units.
 *
 * Makes a canvas scroll to the specified offsets, given in canvas pixel units.
 * The canvas will adjust the view so that it is not outside the scrolling
 * region.  This function is typically not used, as it is better to hook
 * scrollbars to the canvas layout's scrolling adjusments.
 */
void
ganv_canvas_scroll_to(GanvCanvas* canvas, int cx, int cy);

/**
 * ganv_canvas_get_scroll_offsets:
 * @canvas: A canvas.
 * @cx: Horizontal scrolling offset (return value).
 * @cy: Vertical scrolling offset (return value).
 *
 * Queries the scrolling offsets of a canvas.  The values are returned in canvas
 * pixel units.
 */
void
ganv_canvas_get_scroll_offsets(const GanvCanvas* canvas, int* cx, int* cy);

/**
 * ganv_canvas_w2c_affine:
 * @canvas: A canvas.
 * @matrix: An affine transformation matrix (return value).
 *
 * Gets the affine transform that converts from world coordinates to canvas
 * pixel coordinates.
 */
void
ganv_canvas_w2c_affine(GanvCanvas* canvas, cairo_matrix_t* matrix);

/**
 * ganv_canvas_w2c:
 * @canvas: A canvas.
 * @wx: World X coordinate.
 * @wy: World Y coordinate.
 * @cx: X pixel coordinate (return value).
 * @cy: Y pixel coordinate (return value).
 *
 * Converts world coordinates into canvas pixel coordinates.
 */
void
ganv_canvas_w2c(GanvCanvas* canvas, double wx, double wy, int* cx, int* cy);

/**
 * ganv_canvas_w2c_d:
 * @canvas: A canvas.
 * @wx: World X coordinate.
 * @wy: World Y coordinate.
 * @cx: X pixel coordinate (return value).
 * @cy: Y pixel coordinate (return value).
 *
 * Converts world coordinates into canvas pixel coordinates.  This version
 * uses floating point coordinates for greater precision.
 */
void
ganv_canvas_w2c_d(GanvCanvas* canvas,
                  double      wx,
                  double      wy,
                  double*     cx,
                  double*     cy);

/**
 * ganv_canvas_c2w:
 * @canvas: A canvas.
 * @cx: Canvas pixel X coordinate.
 * @cy: Canvas pixel Y coordinate.
 * @wx: X world coordinate (return value).
 * @wy: Y world coordinate (return value).
 *
 * Converts canvas pixel coordinates to world coordinates.
 */
void
ganv_canvas_c2w(GanvCanvas* canvas, int cx, int cy, double* wx, double* wy);

/**
 * ganv_canvas_window_to_world:
 * @canvas: A canvas.
 * @winx: Window-relative X coordinate.
 * @winy: Window-relative Y coordinate.
 * @worldx: X world coordinate (return value).
 * @worldy: Y world coordinate (return value).
 *
 * Converts window-relative coordinates into world coordinates.  You can use
 * this when you need to convert mouse coordinates into world coordinates, for
 * example.
 */
void
ganv_canvas_window_to_world(GanvCanvas* canvas,
                            double      winx,
                            double      winy,
                            double*     worldx,
                            double*     worldy);

/**
 * ganv_canvas_world_to_window:
 * @canvas: A canvas.
 * @worldx: World X coordinate.
 * @worldy: World Y coordinate.
 * @winx: X window-relative coordinate.
 * @winy: Y window-relative coordinate.
 *
 * Converts world coordinates into window-relative coordinates.
 */
void
ganv_canvas_world_to_window(GanvCanvas* canvas,
                            double      worldx,
                            double      worldy,
                            double*     winx,
                            double*     winy);

/**
 * ganv_canvas_get_item_at:
 * @canvas: A canvas.
 * @x: X position in world coordinates.
 * @y: Y position in world coordinates.
 *
 * Looks for the item that is under the specified position, which must be
 * specified in world coordinates.
 *
 * Returns: (transfer none): The sought item, or NULL if no item is at the
 * specified coordinates.
 */
GanvItem*
ganv_canvas_get_item_at(GanvCanvas* canvas, double x, double y);

/**
 * ganv_canvas_get_edge:
 *
 * Get the edge between two nodes, or NULL if none exists.
 *
 * Return value: (transfer none): The root group of @canvas.
 */
GanvEdge*
ganv_canvas_get_edge(GanvCanvas* canvas,
                     GanvNode*   tail,
                     GanvNode*   head);

/**
 * ganv_canvas_remove_edge:
 *
 * Remove @edge from the canvas.
 */
void
ganv_canvas_remove_edge(GanvCanvas* canvas,
                        GanvEdge*   edge);

/**
 * ganv_canvas_remove_edge_between:
 *
 * Remove the edge from @tail to @head if one exists.
 */
void
ganv_canvas_remove_edge_between(GanvCanvas* canvas,
                                GanvNode*   tail,
                                GanvNode*   head);

/**
 * ganv_canvas_get_direction:
 *
 * Return the direction of signal flow.
 */
GanvDirection
ganv_canvas_get_direction(GanvCanvas* canvas);

/**
 * ganv_canvas_set_direction:
 *
 * Set the direction of signal flow.
 */
void
ganv_canvas_set_direction(GanvCanvas* canvas, GanvDirection dir);

/**
 * ganv_canvas_arrange:
 *
 * Automatically arrange the canvas contents.
 */
void
ganv_canvas_arrange(GanvCanvas* canvas);

/**
 * ganv_canvas_export_image:
 *
 * Draw the canvas to an image file.  The file type is determined by extension,
 * currently supported: pdf, ps, svg, dot.
 *
 * Returns: 0 on success.
 */
int
ganv_canvas_export_image(GanvCanvas* canvas,
                         const char* filename,
                         gboolean    draw_background);

/**
 * ganv_canvas_export_dot:
 *
 * Write a Graphviz DOT description of the canvas to a file.
 */
void
ganv_canvas_export_dot(GanvCanvas* canvas, const char* filename);

/**
 * ganv_canvas_supports_sprung_layout:
 *
 * Returns: true iff ganv is compiled with sprung layout support.
 */
gboolean
ganv_canvas_supports_sprung_layout(const GanvCanvas* canvas);

/**
 * ganv_canvas_set_sprung_layout:
 *
 * Enable or disable "live" force-directed canvas layout.
 *
 * Returns: true iff sprung layout was enabled.
 */
gboolean
ganv_canvas_set_sprung_layout(GanvCanvas* canvas, gboolean sprung_layout);

/**
 * ganv_canvas_get_locked:
 *
 * Return true iff the canvas is locked and nodes may not move.
 */
gboolean
ganv_canvas_get_locked(const GanvCanvas* canvas);

/**
 * ganv_canvas_for_each_node:
 * @canvas: The canvas.
 * @f: (scope call): A function to call on every node on @canvas.
 * @data: Data to pass to @f.
 */
void
ganv_canvas_for_each_node(GanvCanvas*  canvas,
                          GanvNodeFunc f,
                          void*        data);

/**
 * ganv_canvas_for_each_selected_node:
 * @canvas: The canvas.
 * @f: (scope call): A function to call on every selected node on @canvas.
 * @data: Data to pass to @f.
 */
void
ganv_canvas_for_each_selected_node(GanvCanvas*  canvas,
                                   GanvNodeFunc f,
                                   void*        data);

/**
 * ganv_canvas_for_each_edge:
 * @canvas: The canvas.
 * @f: (scope call): A function to call on every edge on @canvas.
 * @data: Data to pass to @f.
 */
void
ganv_canvas_for_each_edge(GanvCanvas*  canvas,
                          GanvEdgeFunc f,
                          void*        data);

/**
 * ganv_canvas_for_each_edge_from:
 * @canvas: The canvas.
 * @tail: The tail to enumerate every edge for.
 * @f: (scope call): A function to call on every edge leaving @tail.
 */
void
ganv_canvas_for_each_edge_from(GanvCanvas*     canvas,
                               const GanvNode* tail,
                               GanvEdgeFunc    f,
                               void*           data);

/**
 * ganv_canvas_for_each_edge_to:
 * @canvas: The canvas.
 * @head: The head to enumerate every edge for.
 * @f: (scope call): A function to call on every edge entering @head.
 */
void
ganv_canvas_for_each_edge_to(GanvCanvas*     canvas,
                             const GanvNode* head,
                             GanvEdgeFunc    f,
                             void*           data);

/**
 * ganv_canvas_for_each_edge_on:
 * @canvas: The canvas.
 * @node: The node to enumerate every edge for.
 * @f: (scope call): A function to call on every edge attached to @node.
 */
void
ganv_canvas_for_each_edge_on(GanvCanvas*     canvas,
                             const GanvNode* node,
                             GanvEdgeFunc    f,
                             void*           data);

/**
 * ganv_canvas_for_each_selected_edge:
 * @canvas: The canvas.
 * @f: (scope call): A function to call on every edge attached to @node.
 * @data: Data to pass to @f.
 */
void
ganv_canvas_for_each_selected_edge(GanvCanvas*  canvas,
                                   GanvEdgeFunc f,
                                   void*        data);

/**
 * ganv_canvas_select_all:
 *
 * Select all items on the canvas.
 */
void
ganv_canvas_select_all(GanvCanvas* canvas);

/**
 * ganv_canvas_clear_selection:
 *
 * Deselect any selected items on the canvas.
 */
void
ganv_canvas_clear_selection(GanvCanvas* canvas);

/**
 * ganv_canvas_get_zoom:
 *
 * Return the current zoom factor (pixels per unit).
 */
double
ganv_canvas_get_zoom(const GanvCanvas* canvas);

/**
 * ganv_canvas_set_zoom:
 * @canvas: A canvas.
 * @zoom: The number of pixels that correspond to one canvas unit.
 *
 * The anchor point for zooming, i.e. the point that stays fixed and all others
 * zoom inwards or outwards from it, depends on whether the canvas is set to
 * center the scrolling region or not.  You can control this using the
 * ganv_canvas_set_center_scroll_region() function.  If the canvas is set to
 * center the scroll region, then the center of the canvas window is used as
 * the anchor point for zooming.  Otherwise, the upper-left corner of the
 * canvas window is used as the anchor point.
 */
void
ganv_canvas_set_zoom(GanvCanvas* canvas, double zoom);

/**
 * ganv_canvas_zoom_full:
 *
 * Zoom so all canvas contents are visible.
 */
void
ganv_canvas_zoom_full(GanvCanvas* canvas);

/**
 * ganv_canvas_get_default_font_size:
 *
 * Get the default font size in points.
 */
double
ganv_canvas_get_default_font_size(const GanvCanvas* canvas);

/**
 * ganv_canvas_get_font_size:
 *
 * Get the current font size in points.
 */
double
ganv_canvas_get_font_size(const GanvCanvas* canvas);

/**
 * ganv_canvas_set_font_size:
 *
 * Set the current font size in points.
 */
void
ganv_canvas_set_font_size(GanvCanvas* canvas, double points);

/**
 * ganv_canvas_get_move_cursor:
 *
 * Return the cursor to use while dragging canvas objects.
 */
GdkCursor*
ganv_canvas_get_move_cursor(const GanvCanvas* canvas);

/**
 * ganv_canvas_move_contents_to:
 *
 * Shift all canvas contents so the top-left object is at (x, y).
 */
void
ganv_canvas_move_contents_to(GanvCanvas* canvas, double x, double y);

/**
 * ganv_canvas_set_port_order:
 * @canvas The canvas to set the default port order on.
 * @port_cmp Port comparison function.
 * @data Data to be passed to order.
 *
 * Set a comparator function to use as the default order for ports on modules.
 * If left unset, ports are shown in the order they are added.
 */
void
ganv_canvas_set_port_order(GanvCanvas*       canvas,
                           GanvPortOrderFunc port_cmp,
                           void*             data);


G_END_DECLS

#endif  /* GANV_CANVAS_H */
