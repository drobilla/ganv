/* This file is part of Ganv.
 * Copyright 2007-2016 David Robillard <http://drobilla.net>
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

#include "ganv-private.h"

#include <ganv/box.h>
#include <ganv/canvas.h>
#include <ganv/item.h>
#include <ganv/module.h>
#include <ganv/node.h>
#include <ganv/port.h>
#include <ganv/text.h>
#include <ganv/types.h>
#include <ganv/widget.h>

#include <cairo.h>
#include <glib-object.h>
#include <glib.h>
#include <gobject/gclosure.h>
#include <gtk/gtk.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FOREACH_PORT(ports, i) \
	for (GanvPort** i = (GanvPort**)ports->pdata; \
	     i != (GanvPort**)ports->pdata + ports->len; ++i)

#define FOREACH_PORT_CONST(ports, i) \
	for (const GanvPort** i = (const GanvPort**)ports->pdata; \
	     i != (const GanvPort**)ports->pdata + ports->len; ++i)

static const double PAD              = 2.0;
static const double EDGE_PAD         = 5.0;
static const double MODULE_LABEL_PAD = 2.0;

G_DEFINE_TYPE_WITH_CODE(GanvModule, ganv_module, GANV_TYPE_BOX,
                        G_ADD_PRIVATE(GanvModule))

static GanvBoxClass* parent_class;

enum {
	PROP_0
};

static void
ganv_module_init(GanvModule* module)
{
	GanvModulePrivate* impl =
	    (GanvModulePrivate*)ganv_module_get_instance_private(module);

	module->impl = impl;

	GANV_NODE(module)->impl->can_head = FALSE;
	GANV_NODE(module)->impl->can_tail = FALSE;

	impl->ports = g_ptr_array_new();
	impl->embed_item    = NULL;
	impl->embed_width   = 0;
	impl->embed_height  = 0;
	impl->widest_input  = 0.0;
	impl->widest_output = 0.0;
	impl->must_reorder  = FALSE;
}

static void
ganv_module_destroy(GtkObject* object)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(GANV_IS_MODULE(object));

	GanvModule*        module = GANV_MODULE(object);
	GanvModulePrivate* impl   = module->impl;

	if (impl->ports) {
		FOREACH_PORT(impl->ports, p) {
			g_object_unref(GTK_OBJECT(*p));
		}
		g_ptr_array_free(impl->ports, TRUE);
		impl->ports = NULL;
	}

	if (impl->embed_item) {
		g_object_unref(GTK_OBJECT(impl->embed_item));
		impl->embed_item = NULL;
	}

	if (GTK_OBJECT_CLASS(parent_class)->destroy) {
		(*GTK_OBJECT_CLASS(parent_class)->destroy)(object);
	}
}

static void
ganv_module_set_property(GObject*      object,
                         guint         prop_id,
                         const GValue* value,
                         GParamSpec*   pspec)
{
	(void)value;

	g_return_if_fail(object != NULL);
	g_return_if_fail(GANV_IS_MODULE(object));

	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
ganv_module_get_property(GObject*    object,
                         guint       prop_id,
                         GValue*     value,
                         GParamSpec* pspec)
{
	(void)value;

	g_return_if_fail(object != NULL);
	g_return_if_fail(GANV_IS_MODULE(object));

	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

typedef struct {
	double   embed_x;
	double   width;
	double   input_width;
	double   output_width;
	gboolean horiz;
	gboolean embed_between;
} Metrics;

static void
title_size(GanvModule* module, double* w, double* h)
{
	if (module->box.node.impl->label) {
		g_object_get(G_OBJECT(module->box.node.impl->label),
		             "width", w,
		             "height", h,
		             NULL);
	} else {
		*w = *h = 0.0;
	}
}

static void
measure(GanvModule* module, Metrics* m)
{
	memset(m, '\0', sizeof(Metrics));

	double title_w = 0.0;
	double title_h = 0.0;
	title_size(module, &title_w, &title_h);

	GanvCanvas*        canvas       = ganv_item_get_canvas(GANV_ITEM(module));
	GanvText*          canvas_title = GANV_NODE(module)->impl->label;
	GanvModulePrivate* impl         = module->impl;

	if (ganv_canvas_get_direction(canvas) == GANV_DIRECTION_DOWN) {
		double contents_width = 0.0;
		if (canvas_title) {
			contents_width += title_w + (2.0 * PAD);
		}

		m->embed_x      = 0;
		m->input_width  = ganv_module_get_empty_port_breadth(module);
		m->output_width = ganv_module_get_empty_port_breadth(module);

		// TODO: cache this or merge with resize_right
		unsigned n_inputs  = 0;
		unsigned n_outputs = 0;
		FOREACH_PORT(impl->ports, pi) {
			if ((*pi)->impl->is_input) {
				++n_inputs;
			} else {
				++n_outputs;
			}
		}

		const unsigned hor_ports = MAX(1, MAX(n_inputs, n_outputs));
		const double ports_width = (2 * EDGE_PAD) +
			((m->input_width) * hor_ports) +
			((PAD + 1.0) * (hor_ports - 1));

		m->width = MAX(contents_width, ports_width);
		m->width = MAX(m->width, impl->embed_width);

		if (impl->embed_item) {
			m->width   = MAX(impl->embed_width + 2.0 * PAD, m->width);
			m->embed_x = PAD;
		}
		return;
	}

	// The amount of space between a port edge and the module edge (on the
	// side that the port isn't right on the edge).
	const double hor_pad = (canvas_title ? 10.0 : 20.0);

	m->width = (canvas_title) ? title_w + 10.0 : 1.0;

	// Title is wide or there is an embedded widget,
	// put inputs and outputs beside each other
	m->horiz = (impl->embed_item ||
	            (impl->widest_input + impl->widest_output + 10.0
	             < MAX(m->width, impl->embed_width)));

	// Fit ports to module (or vice-versa)
	m->input_width  = impl->widest_input;
	m->output_width = impl->widest_output;
	double expand_w = (m->horiz ? (m->width / 2.0) : m->width) - hor_pad;
	if (!impl->embed_item) {
		m->input_width  = MAX(impl->widest_input,  expand_w);
		m->output_width = MAX(impl->widest_output, expand_w);
	}

	const double widest = MAX(m->input_width, m->output_width);

	if (impl->embed_item) {
		double above_w   = MAX(m->width, widest + hor_pad);
		double between_w = MAX(m->width,
		                       (m->input_width
		                        + m->output_width
		                        + impl->embed_width));

		above_w = MAX(above_w, impl->embed_width);

		// Decide where to place embedded widget if necessary)
		if (impl->embed_width < impl->embed_height * 2.0) {
			m->embed_between = TRUE;
			m->width         = between_w;
			m->embed_x       = m->input_width;
		} else {
			m->width   = above_w;
			m->embed_x = 2.0;
		}
	}

	if (!canvas_title && (impl->widest_input == 0.0
	                      || impl->widest_output == 0.0)) {
		m->width += 10.0;
	}

	m->width += 4.0;
	m->width = MAX(m->width, widest + hor_pad);
}

static void
place_title(GanvModule* module, GanvDirection dir)
{
	GanvBox*  box          = GANV_BOX(module);
	GanvText* canvas_title = GANV_NODE(module)->impl->label;

	double title_w = 0.0;
	double title_h = 0.0;
	title_size(module, &title_w, &title_h);

	if (!canvas_title) {
		return;
	}

	GanvItem* t = GANV_ITEM(canvas_title);
	if (dir == GANV_DIRECTION_RIGHT) {
		t->impl->x = (ganv_box_get_width(box) - title_w) / 2.0;
		t->impl->y = 1.0;
	} else {
		t->impl->x = (ganv_box_get_width(box) - title_w) / 2.0;
		t->impl->y = ganv_module_get_empty_port_depth(module) + 1.0;
	}
}

static void
resize_right(GanvModule* module)
{
	GanvCanvas*        canvas = ganv_item_get_canvas(GANV_ITEM(module));
	GanvModulePrivate* impl   = module->impl;

	Metrics m;
	measure(module, &m);

	double title_w = 0.0;
	double title_h = 0.0;
	title_size(module, &title_w, &title_h);

	// Basic height contains title
	double header_height = title_h ? (3.0 + title_h) : EDGE_PAD;

	if (impl->embed_item) {
		ganv_item_set(impl->embed_item,
		              "x", (double)m.embed_x,
		              "y", header_height,
		              NULL);
	}

	// Actually set width and height
	ganv_box_set_width(GANV_BOX(module), m.width);

	// Offset ports below embedded widget
	if (!m.embed_between) {
		header_height += impl->embed_height;
	}

	// Move ports to appropriate locations
	double   in_y           = header_height;
	double   out_y          = header_height;
	FOREACH_PORT(impl->ports, pi) {
		GanvPort* const p     = (*pi);
		GanvBox*  const pbox  = GANV_BOX(p);
		GanvNode* const pnode = GANV_NODE(p);
		const double    h     = ganv_box_get_height(pbox);

		// Offset to shift ports to make borders line up
		const double border_off = (GANV_NODE(module)->impl->border_width -
		                           pnode->impl->border_width) / 2.0;

		if (p->impl->is_input) {
			ganv_node_move_to(pnode, -border_off, in_y + 1.0);
			ganv_box_set_width(pbox, m.input_width);
			in_y += h + pnode->impl->border_width + 1.0;

			ganv_canvas_for_each_edge_to(
				canvas, pnode,
				(GanvEdgeFunc)ganv_edge_update_location, NULL);
		} else {
			ganv_node_move_to(pnode, m.width - m.output_width + border_off, out_y + 1.0);
			ganv_box_set_width(pbox, m.output_width);
			out_y += h + pnode->impl->border_width + 1.0;

			ganv_canvas_for_each_edge_from(
				canvas, pnode,
				(GanvEdgeFunc)ganv_edge_update_location, NULL);
		}

		if (!m.horiz) {
			in_y  = MAX(in_y, out_y);
			out_y = MAX(in_y, out_y);
		}
	}

	double height = MAX(in_y, out_y) + EDGE_PAD;
	if (impl->embed_item && m.embed_between)
		height = MAX(height, impl->embed_height + header_height + 2.0);

	ganv_box_set_height(GANV_BOX(module), height);

	place_title(module, GANV_DIRECTION_RIGHT);
}

static void
resize_down(GanvModule* module)
{
	GanvCanvas*        canvas = ganv_item_get_canvas(GANV_ITEM(module));
	GanvModulePrivate* impl   = module->impl;

	Metrics m;
	measure(module, &m);

	double title_w = 0.0;
	double title_h = 0.0;
	title_size(module, &title_w, &title_h);

	const double port_depth   = ganv_module_get_empty_port_depth(module);
	const double port_breadth = ganv_module_get_empty_port_breadth(module);

	if (impl->embed_item) {
		ganv_item_set(impl->embed_item,
		              "x", (double)m.embed_x,
		              "y", port_depth + title_h,
		              NULL);
	}

	const double height = PAD + title_h
		+ impl->embed_height + (port_depth * 2.0);

	// Move ports to appropriate locations
	guint  in_count  = 0;
	guint  out_count = 0;
	double in_x      = 0.0;
	double out_x     = 0.0;
	FOREACH_PORT(impl->ports, pi) {
		GanvPort* const p     = (*pi);
		GanvBox*  const pbox  = GANV_BOX(p);
		GanvNode* const pnode = GANV_NODE(p);
		ganv_box_set_width(pbox, port_breadth);
		ganv_box_set_height(pbox, port_depth);

		// Offset to shift ports to make borders line up
		const double border_off = (GANV_NODE(module)->impl->border_width -
		                           pnode->impl->border_width) / 2.0;

		if (p->impl->is_input) {
			in_x = EDGE_PAD + (in_count++ * (port_breadth + PAD + 1.0));
			ganv_node_move_to(pnode, in_x, -border_off);
			ganv_canvas_for_each_edge_to(
				canvas, pnode,
				(GanvEdgeFunc)ganv_edge_update_location, NULL);
		} else {
			out_x = EDGE_PAD + (out_count++ * (port_breadth + PAD + 1.0));
			ganv_node_move_to(pnode, out_x, height - port_depth + border_off);
			ganv_canvas_for_each_edge_from(
				canvas, pnode,
				(GanvEdgeFunc)ganv_edge_update_location, NULL);
		}
	}

	ganv_box_set_height(GANV_BOX(module), height);
	ganv_box_set_width(GANV_BOX(module), m.width);
	place_title(module, GANV_DIRECTION_DOWN);
}

static void
measure_ports(GanvModule* module)
{
	GanvModulePrivate* impl = module->impl;

	impl->widest_input  = 0.0;
	impl->widest_output = 0.0;
	FOREACH_PORT_CONST(impl->ports, pi) {
		const GanvPort* const p = (*pi);
		const double          w = ganv_port_get_natural_width(p);
		if (p->impl->is_input) {
			if (w > impl->widest_input) {
				impl->widest_input = w;
			}
		} else {
			if (w > impl->widest_output) {
				impl->widest_output = w;
			}
		}
	}
}

static void
ganv_module_resize(GanvNode* self)
{
	GanvModule* module = GANV_MODULE(self);
	GanvNode*   node   = GANV_NODE(self);
	GanvCanvas* canvas = ganv_item_get_canvas(GANV_ITEM(module));

	double label_w = 0.0;
	double label_h = 0.0;
	if (node->impl->label) {
		g_object_get(node->impl->label,
		             "width", &label_w,
		             "height", &label_h,
		             NULL);
	}

	measure_ports(module);

	ganv_box_set_width(GANV_BOX(module), label_w + (MODULE_LABEL_PAD * 2.0));
	ganv_box_set_height(GANV_BOX(module), label_h);

	switch (ganv_canvas_get_direction(canvas)) {
	case GANV_DIRECTION_RIGHT:
		resize_right(module);
		break;
	case GANV_DIRECTION_DOWN:
		resize_down(module);
		break;
	}

	if (GANV_NODE_CLASS(parent_class)->resize) {
		GANV_NODE_CLASS(parent_class)->resize(self);
	}
}

static void
ganv_module_redraw_text(GanvNode* self)
{
	FOREACH_PORT(GANV_MODULE(self)->impl->ports, p) {
		ganv_node_redraw_text(GANV_NODE(*p));
	}

	if (parent_class->parent_class.redraw_text) {
		parent_class->parent_class.redraw_text(self);
	}
}

static void
ganv_module_add_port(GanvModule* module,
                     GanvPort*   port)
{
	GanvModulePrivate* impl = module->impl;

	// Update widest input/output measurements if necessary
	const double width = ganv_port_get_natural_width(port);
	if (port->impl->is_input && width > impl->widest_input) {
		impl->widest_input = width;
	} else if (!port->impl->is_input && width > impl->widest_output) {
		impl->widest_output = width;
	}

	// Add to port array
	g_ptr_array_add(impl->ports, port);

	// Request update with resize and reorder
	GANV_NODE(module)->impl->must_resize = TRUE;
	impl->must_reorder                   = TRUE;
}

static void
ganv_module_remove_port(GanvModule* module,
                        GanvPort*   port)
{
	gboolean removed = g_ptr_array_remove(module->impl->ports, port);
	if (removed) {
		const double width = ganv_box_get_width(GANV_BOX(port));
		// Find new widest input or output, if necessary
		if (port->impl->is_input && width >= module->impl->widest_input) {
			module->impl->widest_input = 0;
			FOREACH_PORT_CONST(module->impl->ports, i) {
				const GanvPort* const p = (*i);
				const double          w = ganv_box_get_width(GANV_BOX(p));
				if (p->impl->is_input && w >= module->impl->widest_input) {
					module->impl->widest_input = w;
				}
			}
		} else if (!port->impl->is_input && width >= module->impl->widest_output) {
			module->impl->widest_output = 0;
			FOREACH_PORT_CONST(module->impl->ports, i) {
				const GanvPort* const p = (*i);
				const double          w = ganv_box_get_width(GANV_BOX(p));
				if (!p->impl->is_input && w >= module->impl->widest_output) {
					module->impl->widest_output = w;
				}
			}
		}

		GANV_NODE(module)->impl->must_resize = TRUE;
	} else {
		fprintf(stderr, "Failed to find port to remove\n");
	}
}

static void
ganv_module_add(GanvItem* item, GanvItem* child)
{
	if (GANV_IS_PORT(child)) {
		ganv_module_add_port(GANV_MODULE(item), GANV_PORT(child));
	}
	ganv_item_request_update(item);
	if (GANV_ITEM_CLASS(parent_class)->add) {
		GANV_ITEM_CLASS(parent_class)->add(item, child);
	}
}

static void
ganv_module_remove(GanvItem* item, GanvItem* child)
{
	if (GANV_IS_PORT(child)) {
		ganv_module_remove_port(GANV_MODULE(item), GANV_PORT(child));
	}
	ganv_item_request_update(item);
	if (GANV_ITEM_CLASS(parent_class)->remove) {
		GANV_ITEM_CLASS(parent_class)->remove(item, child);
	}
}

static int
ptr_sort(const GanvPort** a, const GanvPort** b, const PortOrderCtx* ctx)
{
	return ctx->port_cmp(*a, *b, ctx->data);
}

static void
ganv_module_update(GanvItem* item, int flags)
{
	GanvModule* module = GANV_MODULE(item);
	GanvCanvas* canvas = ganv_item_get_canvas(item);

	if (module->impl->must_reorder) {
		// Sort ports array
		PortOrderCtx ctx = ganv_canvas_get_port_order(canvas);
		if (ctx.port_cmp) {
			g_ptr_array_sort_with_data(module->impl->ports,
			                           (GCompareDataFunc)ptr_sort,

			                           &ctx);
		}
		module->impl->must_reorder = FALSE;
	}

	if (module->impl->embed_item) {
		// Kick the embedded item to update position if we have moved
		ganv_item_move(GANV_ITEM(module->impl->embed_item), 0.0, 0.0);
	}

	FOREACH_PORT(module->impl->ports, p) {
		ganv_item_invoke_update(GANV_ITEM(*p), flags);
	}

	if (module->impl->embed_item) {
		ganv_item_invoke_update(GANV_ITEM(module->impl->embed_item), flags);
	}

	GANV_ITEM_CLASS(parent_class)->update(item, flags);
}

static void
ganv_module_draw(GanvItem* item,
                 cairo_t* cr, double cx, double cy, double cw, double ch)
{
	GanvNode*   node   = GANV_NODE(item);
	GanvModule* module = GANV_MODULE(item);

	// Draw box
	if (GANV_ITEM_CLASS(parent_class)->draw) {
		(*GANV_ITEM_CLASS(parent_class)->draw)(item, cr, cx, cy, cw, ch);
	}

	// Draw label
	if (node->impl->label) {
		GanvItem* label_item = GANV_ITEM(node->impl->label);
		GANV_ITEM_GET_CLASS(label_item)->draw(label_item, cr, cx, cy, cw, ch);
	}

	// Draw ports
	FOREACH_PORT(module->impl->ports, p) {
		GANV_ITEM_GET_CLASS(GANV_ITEM(*p))->draw(
			GANV_ITEM(*p), cr, cx, cy, cw, ch);
	}

	// Draw embed item
	if (module->impl->embed_item) {
		GANV_ITEM_GET_CLASS(module->impl->embed_item)->draw(
			module->impl->embed_item, cr, cx, cy, cw, ch);
	}
}

static void
ganv_module_move_to(GanvNode* node,
                    double    x,
                    double    y)
{
	GanvModule* module = GANV_MODULE(node);
	GANV_NODE_CLASS(parent_class)->move_to(node, x, y);
	FOREACH_PORT(module->impl->ports, p) {
		ganv_node_move(GANV_NODE(*p), 0.0, 0.0);
	}
	if (module->impl->embed_item) {
		ganv_item_move(GANV_ITEM(module->impl->embed_item), 0.0, 0.0);
	}
}

static void
ganv_module_move(GanvNode* node,
                 double    dx,
                 double    dy)
{
	GanvModule* module = GANV_MODULE(node);
	GANV_NODE_CLASS(parent_class)->move(node, dx, dy);
	FOREACH_PORT(module->impl->ports, p) {
		ganv_node_move(GANV_NODE(*p), 0.0, 0.0);
	}
	if (module->impl->embed_item) {
		ganv_item_move(GANV_ITEM(module->impl->embed_item), 0.0, 0.0);
	}
}

static double
ganv_module_point(GanvItem* item, double x, double y, GanvItem** actual_item)
{
	GanvModule* module = GANV_MODULE(item);

	double d = GANV_ITEM_CLASS(parent_class)->point(item, x, y, actual_item);

	if (!*actual_item) {
		// Point is not inside module at all, no point in checking children
		return d;
	}

	FOREACH_PORT(module->impl->ports, p) {
		GanvItem* const port = GANV_ITEM(*p);

		*actual_item = NULL;
		d = GANV_ITEM_GET_CLASS(port)->point(
			port, x - port->impl->x, y - port->impl->y, actual_item);

		if (*actual_item) {
			// Point is inside a port
			return d;
		}
	}

	// Point is inside module, but not a child port
	*actual_item = item;
	return 0.0;
}

static void
ganv_module_class_init(GanvModuleClass* klass)
{
	GObjectClass*   gobject_class = (GObjectClass*)klass;
	GtkObjectClass* object_class  = (GtkObjectClass*)klass;
	GanvItemClass*  item_class    = (GanvItemClass*)klass;
	GanvNodeClass*  node_class    = (GanvNodeClass*)klass;

	parent_class = GANV_BOX_CLASS(g_type_class_peek_parent(klass));

	gobject_class->set_property = ganv_module_set_property;
	gobject_class->get_property = ganv_module_get_property;

	object_class->destroy = ganv_module_destroy;

	item_class->add    = ganv_module_add;
	item_class->remove = ganv_module_remove;
	item_class->update = ganv_module_update;
	item_class->draw   = ganv_module_draw;
	item_class->point  = ganv_module_point;

	node_class->move        = ganv_module_move;
	node_class->move_to     = ganv_module_move_to;
	node_class->resize      = ganv_module_resize;
	node_class->redraw_text = ganv_module_redraw_text;
}

GanvModule*
ganv_module_new(GanvCanvas* canvas,
                const char* first_property_name, ...)
{
	GanvModule* module = GANV_MODULE(
		g_object_new(ganv_module_get_type(), "canvas", canvas, NULL));

	va_list args;
	va_start(args, first_property_name);
	g_object_set_valist(G_OBJECT(module), first_property_name, args);
	va_end(args);

	return module;
}

guint
ganv_module_num_ports(const GanvModule* module)
{
	return module->impl->ports ? module->impl->ports->len : 0;
}

GanvPort*
ganv_module_get_port(GanvModule* module,
                     guint       index)
{
	return (GanvPort*)g_ptr_array_index(module->impl->ports, index);
}

double
ganv_module_get_empty_port_breadth(const GanvModule* module)
{
	return ganv_module_get_empty_port_depth(module) * 2.0;
}

double
ganv_module_get_empty_port_depth(const GanvModule* module)
{
	GanvCanvas* canvas = ganv_item_get_canvas(GANV_ITEM(module));

	return ganv_canvas_get_font_size(canvas) * 1.1;
}

static void
on_embed_size_request(GtkWidget*      widget,
                      GtkRequisition* r,
                      void*           user_data)
{
	GanvModule*        module = GANV_MODULE(user_data);
	GanvModulePrivate* impl   = module->impl;
	if (impl->embed_width == r->width && impl->embed_height == r->height) {
		return;
	}

	impl->embed_width                    = r->width;
	impl->embed_height                   = r->height;
	GANV_NODE(module)->impl->must_resize = TRUE;

	GtkAllocation allocation;
	allocation.width = r->width;
	allocation.height = r->width;

	gtk_widget_size_allocate(widget, &allocation);
	ganv_item_set(impl->embed_item,
	              "width", (double)r->width,
	              "height", (double)r->height,
	              NULL);
}

void
ganv_module_embed(GanvModule* module,
                  GtkWidget*  widget)
{
	GanvModulePrivate* impl = module->impl;
	if (!widget && !impl->embed_item) {
		return;
	}

	if (impl->embed_item) {
		// Free existing embedded widget
		gtk_object_destroy(GTK_OBJECT(impl->embed_item));
		impl->embed_item = NULL;
	}

	if (!widget) {
		// Removing an existing embedded widget
		impl->embed_width                    = 0;
		impl->embed_height                   = 0;
		GANV_NODE(module)->impl->must_resize = TRUE;
		ganv_item_request_update(GANV_ITEM(module));
		return;
	}

	double title_w = 0.0;
	double title_h = 0.0;
	title_size(module, &title_w, &title_h);

	impl->embed_item = ganv_item_new(
		GANV_ITEM(module),
		ganv_widget_get_type(),
		"x", 2.0,
		"y", 4.0 + title_h,
		"widget", widget,
		NULL);

	GtkRequisition r;
	gtk_widget_show_all(widget);
	gtk_widget_size_request(widget, &r);
	on_embed_size_request(widget, &r, module);
	ganv_item_show(impl->embed_item);

	g_signal_connect(widget, "size-request",
	                 G_CALLBACK(on_embed_size_request), module);

	GANV_NODE(module)->impl->must_resize = TRUE;
	ganv_item_request_update(GANV_ITEM(module));
}

void
ganv_module_set_direction(GanvModule*   module,
                          GanvDirection direction)
{
	FOREACH_PORT(module->impl->ports, p) {
		ganv_port_set_direction(*p, direction);
	}
	GANV_NODE(module)->impl->must_resize = TRUE;
	ganv_item_request_update(GANV_ITEM(module));
}

void
ganv_module_for_each_port(GanvModule*  module,
                          GanvPortFunc f,
                          void*        data)
{
	GanvModulePrivate* impl = module->impl;
	const int          len  = impl->ports->len;
	GanvPort**         copy = (GanvPort**)malloc(sizeof(GanvPort*) * len);
	memcpy(copy, impl->ports->pdata, sizeof(GanvPort*) * len);

	for (int i = 0; i < len; ++i) {
		f(copy[i], data);
	}

	free(copy);
}
