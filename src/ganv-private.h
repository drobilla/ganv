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

#ifndef GANV_PRIVATE_H
#define GANV_PRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <cairo.h>

#include "ganv/canvas.h"
#include "ganv/text.h"
#include "ganv/types.h"

#define GANV_CLOSE_ENOUGH 1

extern guint signal_moved;

/* Box */

typedef struct {
	double   x1, y1, x2, y2;
	double   border_width;
	gboolean stacked;
} GanvBoxCoords;

struct _GanvBoxImpl {
	GanvBoxCoords coords;
	GanvBoxCoords old_coords;
	double        radius_tl;
	double        radius_tr;
	double        radius_br;
	double        radius_bl;
};

/* Circle */

typedef struct {
	double x, y, radius, radius_ems;
	double width;
} GanvCircleCoords;

struct _GanvCircleImpl {
	GanvCircleCoords coords;
	GanvCircleCoords old_coords;
	gboolean         fit_label;
};

/* Edge */

typedef struct {
	double   x1, y1, x2, y2;
	double   cx1, cy1, cx2, cy2;
	double   handle_x, handle_y, handle_radius;
	double   width;
	gboolean curved;
	gboolean arrowhead;
} GanvEdgeCoords;

struct _GanvEdgeImpl
{
	GanvNode*       tail;
	GanvNode*       head;
	GanvEdgeCoords  coords;
	GanvEdgeCoords  old_coords;
	double          dash_length;
	double          dash_offset;
	guint           color;
	gboolean        selected;
	gboolean        highlighted;
	gboolean        ghost;
};

/* Module */

struct _GanvModuleImpl
{
	GPtrArray* ports;
	GanvItem*  embed_item;
	int        embed_width;
	int        embed_height;
	double     widest_input;
	double     widest_output;
	gboolean   must_resize;
};

/* Node */

#ifdef GANV_FDGL
typedef struct {
	double x;
	double y;
} Vector;
#endif

struct _GanvNodeImpl {
	struct _GanvNode* partner;
	GanvText*         label;
	double            dash_length;
	double            dash_offset;
	double            border_width;
	guint             fill_color;
	guint             border_color;
	gboolean          can_tail;
	gboolean          can_head;
	gboolean          is_source;
	gboolean          selected;
	gboolean          highlighted;
	gboolean          draggable;
	gboolean          show_label;
	gboolean          grabbed;
#ifdef GANV_FDGL
	Vector            force;
	Vector            vel;
	gboolean          connected;
#endif
};

/* Widget */

struct _GanvWidgetImpl {
	GtkWidget* widget;          /* The child widget */

	double        x, y;			/* Position at anchor */
	double        width, height; /* Dimensions of widget */
	GtkAnchorType anchor;		/* Anchor side for widget */

	int cx, cy;                 /* Top-left canvas coordinates for widget */
	int cwidth, cheight;		/* Size of widget in pixels */

	guint destroy_id;           /* Signal connection id for destruction of child widget */

	guint size_pixels : 1;		/* Is size specified in (unchanging) pixels or units (get scaled)? */
	guint in_destroy : 1;		/* Is child widget being destroyed? */
};

/* Group */
struct _GanvGroupImpl {
	GList* item_list;
	GList* item_list_end;
};

/* Item */
struct _GanvItemImpl {
	/* Parent canvas for this item */
	struct _GanvCanvas* canvas;

	/* Parent for this item */
	GanvItem* parent;

	/* Layer (z order), higher values are on top */
	guint layer;

	/* Position in parent-relative coordinates. */
	double x, y;

	/* Bounding box for this item (in world coordinates) */
	double x1, y1, x2, y2;

	/* True if parent manages this item (don't call add/remove) */
	gboolean managed;
};

void
ganv_node_tick(GanvNode* self, double seconds);

void
ganv_node_tail_vector(const GanvNode* self,
                      const GanvNode* head,
                      double*         x1,
                      double*         y1,
                      double*         x2,
                      double*         y2);

void
ganv_node_head_vector(const GanvNode* self,
                      const GanvNode* tail,
                      double*         x1,
                      double*         y1,
                      double*         x2,
                      double*         y2);

/**
 * ganv_node_get_draw_properties:
 *
 * Get the colours that should currently be used for drawing this node.  Note
 * these may not be identical to the property values because of highlighting
 * and selection.
 */
void
ganv_node_get_draw_properties(const GanvNode* node,
                              double*         dash_length,
                              double*         border_color,
                              double*         fill_color);

/* Port */

typedef struct {
	GanvBox*  rect;
	GanvText* label;
	float     value;
	float     min;
	float     max;
	gboolean  is_toggle;
	gboolean  is_integer;
} GanvPortControl;

struct _GanvPortImpl {
	GanvPortControl* control;
	gboolean         is_input;
	gboolean         is_controllable;
};

/* Text */

typedef struct
{
	double x;
	double y;
	double width;
	double height;
} GanvTextCoords;

struct _GanvTextImpl
{
	PangoLayout*     layout;
	char*            text;
	GanvTextCoords   coords;
	GanvTextCoords   old_coords;
	guint            color;
	gboolean         needs_layout;
};

/* Canvas */

void
ganv_canvas_move_selected_items(GanvCanvas* canvas,
                                double      dx,
                                double      dy);

void
ganv_canvas_selection_move_finished(GanvCanvas* canvas);

void
ganv_canvas_add_node(GanvCanvas* canvas,
                     GanvNode*   node);

void
ganv_canvas_remove_node(GanvCanvas* canvas,
                        GanvNode*   node);

void
ganv_canvas_select_node(GanvCanvas* canvas,
                        GanvNode*   node);

void
ganv_canvas_unselect_node(GanvCanvas* canvas,
                          GanvNode*   node);

void
ganv_canvas_add_edge(GanvCanvas* canvas,
                     GanvEdge*   edge);

void
ganv_canvas_select_edge(GanvCanvas* canvas,
                        GanvEdge*   edge);

void
ganv_canvas_unselect_edge(GanvCanvas* canvas,
                          GanvEdge*   edge);

void
ganv_canvas_disconnect_edge(GanvCanvas* canvas,
                            GanvEdge*   edge);

gboolean
ganv_canvas_port_event(GanvCanvas* canvas,
                       GanvPort*   port,
                       GdkEvent*   event);

void
ganv_canvas_contents_changed(GanvCanvas* canvas);

void
ganv_item_i2w_offset(GanvItem* item, double* px, double* py);

void
ganv_item_i2w_pair(GanvItem* item, double* x1, double* y1, double* x2, double* y2);

void
ganv_item_invoke_update(GanvItem* item, int flags);

void
ganv_item_emit_event(GanvItem* item, GdkEvent* event, gint* finished);

void
ganv_canvas_request_update(GanvCanvas* canvas);

int
ganv_canvas_emit_event(GanvCanvas* canvas, GdkEvent* event);

void
ganv_canvas_set_need_repick(GanvCanvas* canvas);

void
ganv_canvas_forget_item(GanvCanvas* canvas, GanvItem* item);

void
ganv_canvas_grab_focus(GanvCanvas* canvas, GanvItem* item);

void
ganv_canvas_get_zoom_offsets(GanvCanvas* canvas, int* x, int* y);

int
ganv_canvas_grab_item(GanvItem* item, guint event_mask, GdkCursor* cursor, guint32 etime);

void
ganv_canvas_ungrab_item(GanvItem* item, guint32 etime);

/* Request a redraw of the specified rectangle in canvas coordinates */
void
ganv_canvas_request_redraw_c(GanvCanvas* canvas,
                             int x1, int y1, int x2, int y2);

/* Request a redraw of the specified rectangle in world coordinates */
void
ganv_canvas_request_redraw_w(GanvCanvas* canvas,
                             double x1, double y1, double x2, double y2);

/* Edge */

void
ganv_edge_update_location(GanvEdge* edge);

void
ganv_edge_get_coords(const GanvEdge* edge, GanvEdgeCoords* coords);

void
ganv_edge_request_redraw(GanvItem*             item,
                         const GanvEdgeCoords* coords);

void
ganv_edge_tick(GanvEdge* edge, double seconds);

/* Box */

void
ganv_box_request_redraw(GanvItem*            item,
                        const GanvBoxCoords* coords,
                        gboolean             world);

/* Port */

void
ganv_port_set_control_value_internal(GanvPort* port,
                                     float     value);

void
ganv_port_set_direction(GanvPort*     port,
                        GanvDirection direction);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* GANV_PRIVATE_H */
