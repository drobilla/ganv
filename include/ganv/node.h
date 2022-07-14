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

// IWYU pragma: no_include "ganv-private.h"

#ifndef GANV_NODE_H
#define GANV_NODE_H

#include "ganv/item.h"
#include "ganv/types.h"

#include <glib-object.h>
#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GANV_TYPE_NODE            (ganv_node_get_type())
#define GANV_NODE(obj)            (GTK_CHECK_CAST((obj), GANV_TYPE_NODE, GanvNode))
#define GANV_NODE_CLASS(klass)    (GTK_CHECK_CLASS_CAST((klass), GANV_TYPE_NODE, GanvNodeClass))
#define GANV_IS_NODE(obj)         (GTK_CHECK_TYPE((obj), GANV_TYPE_NODE))
#define GANV_IS_NODE_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GANV_TYPE_NODE))
#define GANV_NODE_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS((obj), GANV_TYPE_NODE, GanvNodeClass))

struct _GanvNodeClass;

typedef struct _GanvNodeClass   GanvNodeClass;
typedef struct _GanvNodePrivate GanvNodePrivate;

struct _GanvNode {
	GanvItem         item;
	GanvNodePrivate* impl;
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

	/* Reserved for future expansion */
	gpointer spare_vmethods[4];
};

GType ganv_node_get_type(void) G_GNUC_CONST;

/**
 * ganv_node_can_tail:
 *
 * Return value: True iff node can act as the tail of an edge.
 */
gboolean
ganv_node_can_tail(const GanvNode* node);

/**
 * ganv_node_can_head:
 *
 * Return value: True iff node can act as the head of an edge.
 */
gboolean
ganv_node_can_head(const GanvNode* node);

/**
 * ganv_node_set_is_source:
 *
 * Flag a node as a source.  This information is used to influence layout.
 */
void
ganv_node_set_is_source(const GanvNode* node, gboolean is_source);

/**
 * ganv_node_is_within:
 *
 * Return value: True iff node is entirely within the given rectangle.
 */
gboolean
ganv_node_is_within(const GanvNode* node,
                    double          x1,
                    double          y1,
                    double          x2,
                    double          y2);

const char* ganv_node_get_label(const GanvNode* node);

double ganv_node_get_border_width(const GanvNode* node);

void ganv_node_set_border_width(const GanvNode* node, double border_width);

double ganv_node_get_dash_length(const GanvNode* node);

void ganv_node_set_dash_length(const GanvNode* node, double dash_length);

double ganv_node_get_dash_offset(const GanvNode* node);

void ganv_node_set_dash_offset(const GanvNode* node, double dash_offset);

guint ganv_node_get_fill_color(const GanvNode* node);

void ganv_node_set_fill_color(const GanvNode* node, guint fill_color);

guint ganv_node_get_border_color(const GanvNode* node);

void ganv_node_set_border_color(const GanvNode* node, guint border_color);

/**
 * ganv_node_get_partner:
 *
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
