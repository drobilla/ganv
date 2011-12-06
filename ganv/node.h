/* This file is part of Ganv.
 * Copyright 2007-2011 David Robillard <http://drobilla.net>
 *
 * Ganv is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Ganv is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Ganv.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GANV_NODE_H
#define GANV_NODE_H

#include <libgnomecanvas/libgnomecanvas.h>

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

struct _GanvNode {
	GnomeCanvasGroup  group;
	struct _GanvNode* partner;
	GanvText*         label;
	double            dash_length;
	double            dash_offset;
	double            border_width;
	guint             fill_color;
	guint             border_color;
	gboolean          can_tail;
	gboolean          can_head;
	gboolean          selected;
	gboolean          highlighted;
	gboolean          draggable;
};

struct _GanvNodeClass {
	GnomeCanvasGroupClass parent_class;

	void (* tick)(GanvNode* self,
	              double    seconds);

	void (* move)(GanvNode* node,
	              double    dx,
	              double    dy);

	void (* move_to)(GanvNode* node,
	                 double    x,
	                 double    y);

	void (* resize)(GanvNode* node);

	void (* disconnect)(GanvNode* node);

	gboolean (* is_within)(const GanvNode* self,
	                       double          x1,
	                       double          y1,
	                       double          x2,
	                       double          y2);


	void (* tail_vector)(const GanvNode* self,
	                     const GanvNode* head,
	                     double*         x,
	                     double*         y,
	                     double*         dx,
	                     double*         dy);

	void (* head_vector)(const GanvNode* self,
	                     const GanvNode* tail,
	                     double*         x,
	                     double*         y,
	                     double*         dx,
	                     double*         dy);

	gboolean (* on_event)(GanvNode* node,
	                      GdkEvent* event);
};

GType ganv_node_get_type(void);

static inline gboolean
ganv_node_can_tail(const GanvNode* self)
{
	return self->can_tail;
}

static inline gboolean
ganv_node_can_head(const GanvNode* self)
{
	return self->can_head;
}

static inline gboolean
ganv_node_is_within(const GanvNode* self,
                    double          x1,
                    double          y1,
                    double          x2,
                    double          y2)
{
	return GANV_NODE_GET_CLASS(self)->is_within(
		self, x1, y1, x2, y2);
}

static inline void
ganv_node_tick(GanvNode* self,
               double    seconds)
{
	GanvNodeClass* klass = GANV_NODE_GET_CLASS(self);
	if (klass->tick) {
		klass->tick(self, seconds);
	}
}

static inline void
ganv_node_tail_vector(const GanvNode* self,
                      const GanvNode* head,
                      double*         x1,
                      double*         y1,
                      double*         x2,
                      double*         y2)
{
	GANV_NODE_GET_CLASS(self)->tail_vector(
		self, head, x1, y1, x2, y2);
}

static inline void
ganv_node_head_vector(const GanvNode* self,
                      const GanvNode* tail,
                      double*         x1,
                      double*         y1,
                      double*         x2,
                      double*         y2)
{
	GANV_NODE_GET_CLASS(self)->head_vector(
		self, tail, x1, y1, x2, y2);
}

/**
   Get the colours that should currently be used for drawing this node.
   Note these may not be identical to the property values because of
   highlighting and selection.
*/
void ganv_node_get_draw_properties(const GanvNode* node,
                                   double*         dash_length,
                                   double*         border_color,
                                   double*         fill_color);

const char* ganv_node_get_label(const GanvNode* node);

static inline GanvNode*
ganv_node_get_partner(const GanvNode* node)
{
	return node->partner;
}

void ganv_node_set_label(GanvNode*   node,
                         const char* str);

static inline void
ganv_node_move(GanvNode* node,
               double    dx,
               double    dy)
{
	GANV_NODE_GET_CLASS(node)->move(node, dx, dy);
}

static inline void
ganv_node_move_to(GanvNode* node,
                  double    x,
                  double    y)
{
	GANV_NODE_GET_CLASS(node)->move_to(node, x, y);
}

static inline void
ganv_node_resize(GanvNode* node)
{
	GanvNodeClass* klass = GANV_NODE_GET_CLASS(node);
	if (klass->resize) {
		klass->resize(node);
	}
}

static inline void
ganv_node_disconnect(GanvNode* node)
{
	GANV_NODE_GET_CLASS(node)->disconnect(node);
}

G_END_DECLS

#endif  /* GANV_NODE_H */
