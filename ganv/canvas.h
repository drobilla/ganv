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

#ifndef GANV_CANVAS_H
#define GANV_CANVAS_H

#include "ganv/canvas-base.h"
#include "ganv/types.h"
#include "ganv/edge.h"

G_BEGIN_DECLS

#define GANV_TYPE_CANVAS            (ganv_canvas_get_type())
#define GANV_CANVAS(obj)            (GTK_CHECK_CAST((obj), GANV_TYPE_CANVAS, GanvCanvas))
#define GANV_CANVAS_CLASS(klass)    (GTK_CHECK_CLASS_CAST((klass), GANV_TYPE_CANVAS, GanvCanvasClass))
#define GANV_IS_CANVAS(obj)         (GTK_CHECK_TYPE((obj), GANV_TYPE_CANVAS))
#define GANV_IS_CANVAS_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GANV_TYPE_CANVAS))
#define GANV_CANVAS_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS((obj), GANV_TYPE_CANVAS, GanvCanvasClass))

struct GanvCanvasImpl;

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
	GanvCanvasBase         canvas;
	struct GanvCanvasImpl* impl;
	GanvDirection          direction;
	double                 width;
	double                 height;
	gboolean               locked;
};

struct _GanvCanvasClass {
	GanvCanvasBaseClass parent_class;
};

GType ganv_canvas_get_type(void);

GanvCanvas* ganv_canvas_new(double width, double height);

/**
 * ganv_canvas_resize:
 * Resize the canvas to the given dimensions.
 */
void
ganv_canvas_resize(GanvCanvas* canvas, double width, double height);

/**
 * ganv_canvas_get_root:
 * Return value: (transfer none): The root group of @canvas.
 */
GanvItem*
ganv_canvas_get_root(const GanvCanvas* canvas);

void
ganv_canvas_add_node(GanvCanvas* canvas,
                     GanvNode*   node);

/**
 * ganv_canvas_get_edge:
 * Get the edge between two nodes, or NULL if none exists.
 * Return value: (transfer none): The root group of @canvas.
 */
GanvEdge*
ganv_canvas_get_edge(GanvCanvas* canvas,
                     GanvNode*   tail,
                     GanvNode*   head);

void
ganv_canvas_disconnect_edge(GanvCanvas* canvas,
                            GanvEdge*   edge);

void
ganv_canvas_remove_edge_between(GanvCanvas* canvas,
                                GanvNode*   tail,
                                GanvNode*   head);

/** Get the default font size in points. */
double
ganv_canvas_get_default_font_size(const GanvCanvas* canvas);

void
ganv_canvas_set_direction(GanvCanvas* canvas, GanvDirection dir);

void
ganv_canvas_clear_selection(GanvCanvas* canvas);

void
ganv_canvas_arrange(GanvCanvas* canvas);

/** Write a Graphviz DOT description of the canvas to @c filename. */
void
ganv_canvas_export_dot(GanvCanvas* canvas, const char* filename);

typedef void (*GanvNodeFunc)(GanvNode* node, void* data);

typedef void (*GanvEdgeFunc)(GanvEdge* edge);

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
                               GanvEdgeFunc    f);

/**
 * ganv_canvas_for_each_edge_to:
 * @canvas: The canvas.
 * @head: The head to enumerate every edge for.
 * @f: (scope call): A function to call on every edge entering @head.
 */
void
ganv_canvas_for_each_edge_to(GanvCanvas*     canvas,
                             const GanvNode* head,
                             GanvEdgeFunc    f);

/**
 * ganv_canvas_for_each_edge_on:
 * @canvas: The canvas.
 * @node: The node to enumerate every edge for.
 * @f: (scope call): A function to call on every edge attached to @node.
 */
void
ganv_canvas_for_each_edge_on(GanvCanvas*     canvas,
                             const GanvNode* node,
                             GanvEdgeFunc    f);

/**
 * ganv_canvas_destroy:
 * Remove all items from the canvas.
 */
void
ganv_canvas_destroy(GanvCanvas* canvas);

/**
 * ganv_canvas_select_all:
 * Select all items on the canvas.
 */
void
ganv_canvas_select_all(GanvCanvas* canvas);

/**
 * ganv_canvas_get_zoom:
 * Return the current zoom factor (pixels per unit).
 */
double
ganv_canvas_get_zoom(GanvCanvas* canvas);

/**
 * ganv_canvas_set_zoom:
 * Set the current zoom factor (pixels per unit).
 */
void
ganv_canvas_set_zoom(GanvCanvas* canvas, double zoom);

/**
 * ganv_canvas_get_font_size:
 * Get the current font size in points.
 */
double
ganv_canvas_get_font_size(const GanvCanvas* canvas);

/**
 * ganv_canvas_set_font_size:
 * Set the current font size in points.
 */
void
ganv_canvas_set_font_size(GanvCanvas* canvas, double points);

/**
 * ganv_canvas_set_scale:
 * Set the zoom and font size for the canvas.
 */
void
ganv_canvas_set_scale(GanvCanvas* canvas, double zoom, double points);

/**
 * ganv_canvas_zoom_full:
 * Zoom so all canvas contents are visible.
 */
void
ganv_canvas_zoom_full(GanvCanvas* canvas);

/**
 * ganv_canvas_get_move_cursor:
 * Return the cursor to use while dragging canvas objects.
 */
GdkCursor*
ganv_canvas_get_move_cursor(const GanvCanvas* canvas);

/**
 * ganv_canvas_move_contents_to:
 * Shift all canvas contents so the top-left object is at (x, y).
 */
void
ganv_canvas_move_contents_to(GanvCanvas* canvas, double x, double y);

G_END_DECLS

#endif  /* GANV_CANVAS_H */
