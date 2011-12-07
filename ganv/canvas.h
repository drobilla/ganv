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

#ifndef GANV_CANVAS_H
#define GANV_CANVAS_H

#include <libgnomecanvas/libgnomecanvas.h>

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

typedef enum {
	GANV_VERTICAL,
	GANV_HORIZONTAL
} GanvDirection;

struct _GanvCanvas
{
	GnomeCanvas            canvas;
	struct GanvCanvasImpl* impl;
	GanvDirection          direction;
	double                 width;
	double                 height;
	gboolean               locked;
};

struct _GanvCanvasClass {
	GnomeCanvasClass parent_class;
};

GType ganv_canvas_get_type(void);

GanvCanvas* ganv_canvas_new(double width, double height);

void
ganv_canvas_resize(GanvCanvas* canvas, double width, double height);

GnomeCanvasGroup*
ganv_canvas_get_root(const GanvCanvas* canvas);

void
ganv_canvas_add_node(GanvCanvas* canvas,
                     GanvNode*   node);

/** Get the default font size in points. */
double
ganv_canvas_get_default_font_size(const GanvCanvas* canvas);

/** Get the current font size in points. */
double
ganv_canvas_get_font_size(const GanvCanvas* canvas);

void
ganv_canvas_set_font_size(GanvCanvas* canvas, double points);

void
ganv_canvas_clear_selection(GanvCanvas* canvas);

typedef void (*GanvEdgeFunction)(GanvEdge* edge);

/**
 * ganv_canvas_for_each_edge_from:
 * @f: (scope call): A function to call on every edge leaving @tail.
 */
void
ganv_canvas_for_each_edge_from(GanvCanvas*      canvas,
                               const GanvNode*  tail,
                               GanvEdgeFunction f);

/**
 * ganv_canvas_for_each_edge_to:
 * @f: (scope call): A function to call on every edge entering @head.
 */
void
ganv_canvas_for_each_edge_to(GanvCanvas*      canvas,
                             const GanvNode*  head,
                             GanvEdgeFunction f);

/**
 * ganv_canvas_for_each_edge_on:
 * @f: (scope call): A function to call on every edge attached to @node.
 */
void
ganv_canvas_for_each_edge_on(GanvCanvas*      canvas,
                             const GanvNode*  node,
                             GanvEdgeFunction f);

G_END_DECLS

#endif  /* GANV_CANVAS_H */
