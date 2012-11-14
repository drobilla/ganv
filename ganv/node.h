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

#ifndef GANV_NODE_H
#define GANV_NODE_H

#include "ganv/canvas-base.h"
#include "ganv/types.h"
#include "ganv/text.h"

G_BEGIN_DECLS

#define GANV_TYPE_NODE            (ganv_node_get_type())
#define GANV_NODE(obj)            (GTK_CHECK_CAST((obj), GANV_TYPE_NODE, GanvNode))
#define GANV_NODE_CLASS(klass)    (GTK_CHECK_CLASS_CAST((klass), GANV_TYPE_NODE, GanvNodeClass))
#define GANV_IS_NODE(obj)         (GTK_CHECK_TYPE((obj), GANV_TYPE_NODE))
#define GANV_IS_NODE_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GANV_TYPE_NODE))
#define GANV_NODE_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS((obj), GANV_TYPE_NODE, GanvNodeClass))

typedef struct _GanvNodeClass GanvNodeClass;
typedef struct _GanvNodeImpl GanvNodeImpl;

struct _GanvNode {
	GanvItem      item;
	GanvNodeImpl* impl;
};

struct _GanvNodeClass {
	GanvItemClass parent_class;

	void (*tick)(GanvNode* self,
	             double    seconds);

	void (*move)(GanvNode* node,
	             double    dx,
	             double    dy);

	void (*move_to)(GanvNode* node,
	                double    x,
	                double    y);

	void (*resize)(GanvNode* node);

	void (*redraw_text)(GanvNode* node);

	void (*disconnect)(GanvNode* node);

	gboolean (*is_within)(const GanvNode* self,
	                      double          x1,
	                      double          y1,
	                      double          x2,
	                      double          y2);

	void (*tail_vector)(const GanvNode* self,
	                    const GanvNode* head,
	                    double*         x,
	                    double*         y,
	                    double*         dx,
	                    double*         dy);

	void (*head_vector)(const GanvNode* self,
	                    const GanvNode* tail,
	                    double*         x,
	                    double*         y,
	                    double*         dx,
	                    double*         dy);
};

GType ganv_node_get_type(void);

gboolean
ganv_node_can_tail(const GanvNode* self);

gboolean
ganv_node_can_head(const GanvNode* self);

gboolean
ganv_node_is_within(const GanvNode* self,
                    double          x1,
                    double          y1,
                    double          x2,
                    double          y2);

void
ganv_node_tick(GanvNode* self,
               double    seconds);

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

const char* ganv_node_get_label(const GanvNode* node);

/**
 * ganv_node_get_partner:
 * Return value: (transfer none): The partner of @node.
 */
GanvNode*
ganv_node_get_partner(const GanvNode* node);

void ganv_node_set_label(GanvNode* node, const char* str);
void ganv_node_set_show_label(GanvNode* node, gboolean show);

void
ganv_node_move(GanvNode* node,
               double    dx,
               double    dy);

void
ganv_node_move_to(GanvNode* node,
                  double    x,
                  double    y);

void
ganv_node_resize(GanvNode* node);

void
ganv_node_redraw_text(GanvNode* node);

void
ganv_node_disconnect(GanvNode* node);

gboolean
ganv_node_is_selected(GanvNode* node);

G_END_DECLS

#endif  /* GANV_NODE_H */
