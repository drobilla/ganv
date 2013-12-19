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

#include <stdlib.h>
#include <string.h>

#include "ganv/canvas.h"
#include "ganv/module.h"
#include "ganv/port.h"
#include "ganv/widget.h"

#include "./color.h"
#include "./boilerplate.h"
#include "./gettext.h"
#include "./ganv-private.h"

#define FOREACH_PORT(ports, i) \
	for (GanvPort** i = (GanvPort**)ports->pdata; \
	     i != (GanvPort**)ports->pdata + ports->len; ++i)

#define FOREACH_PORT_CONST(ports, i) \
	for (const GanvPort** i = (const GanvPort**)ports->pdata; \
	     i != (const GanvPort**)ports->pdata + ports->len; ++i)

static const double MODULE_LABEL_PAD = 2.0;
static const double MODULE_ICON_SIZE = 16;

G_DEFINE_TYPE(GanvModule, ganv_module, GANV_TYPE_BOX)

static GanvBoxClass* parent_class;

enum {
	PROP_0,
};

static void
ganv_module_init(GanvModule* module)
{
	GanvModuleImpl* impl = G_TYPE_INSTANCE_GET_PRIVATE(
		module, GANV_TYPE_MODULE, GanvModuleImpl);

	module->impl = impl;

	GANV_NODE(module)->impl->can_head = FALSE;
	GANV_NODE(module)->impl->can_tail = FALSE;

	impl->ports = g_ptr_array_new();
	impl->embed_item    = NULL;
	impl->embed_width   = 0;
	impl->embed_height  = 0;
	impl->widest_input  = 0.0;
	impl->widest_output = 0.0;
	impl->must_resize   = TRUE;
}

static void
ganv_module_destroy(GtkObject* object)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(GANV_IS_MODULE(object));

	GanvModule*     module = GANV_MODULE(object);
	GanvModuleImpl* impl   = module->impl;

	if (impl->ports) {
		FOREACH_PORT(impl->ports, p) {
			g_object_unref(GTK_OBJECT(*p));
		}
		g_ptr_array_free(impl->ports, TRUE);
		impl->ports = NULL;
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

	double title_w, title_h;
	title_size(module, &title_w, &title_h);

	GanvCanvas*     canvas       = GANV_CANVAS(GANV_ITEM(module)->canvas);
	GanvText*       canvas_title = GANV_NODE(module)->impl->label;
	GanvModuleImpl* impl         = module->impl;

	if (canvas->direction == GANV_DIRECTION_DOWN) {
		static const double PAD = 2.0;

		double contents_width = PAD;
		if (canvas_title) {
			contents_width += title_w;
		}

		m->embed_x      = 0;
		m->input_width  = ganv_module_get_empty_port_breadth(module);
		m->output_width = ganv_module_get_empty_port_breadth(module);

		const double ports_width = PAD + ((m->input_width + PAD) * impl->ports->len);

		m->width = MAX(contents_width, ports_width);
		m->width = MAX(m->width, impl->embed_width);
		return;
	}

	// The amount of space between a port edge and the module edge (on the
	// side that the port isn't right on the edge).
	const double hor_pad = (canvas_title ? 10.0 : 20.0);

	m->width = (canvas_title)
		? title_w + 10.0
		: 1.0;

	// Title is wide, put inputs and outputs beside each other
	m->horiz = (impl->widest_input + impl->widest_output + 10.0
	            < MAX(m->width, impl->embed_width));

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

	double title_w, title_h;
	title_size(module, &title_w, &title_h);

	if (!canvas_title) {
		return;
	} else if (dir == GANV_DIRECTION_RIGHT) {
		ganv_item_set(GANV_ITEM(canvas_title),
		              "x", (ganv_box_get_width(box) - title_w) / 2.0,
		              "y", 2.0,
		              NULL);
	} else {
		ganv_item_set(GANV_ITEM(canvas_title),
		              "x", (ganv_box_get_width(box) - title_w) / 2.0,
		              "y", ganv_module_get_empty_port_depth(module) + 1.0,
		              NULL);
	}
}

static void
resize_right(GanvModule* module)
{
	GanvCanvas*     canvas = GANV_CANVAS(GANV_ITEM(module)->canvas);
	GanvModuleImpl* impl   = module->impl;

	Metrics m;
	measure(module, &m);

	double title_w, title_h;
	title_size(module, &title_w, &title_h);

	// Basic height contains title
	double header_height = 2.0 + title_h;

	if (impl->embed_item) {
		ganv_item_set(impl->embed_item,
		              "x", (double)m.embed_x,
		              NULL);
	}

	// Actually set width and height
	ganv_box_set_width(GANV_BOX(module), m.width);

	// Offset ports below embedded widget
	if (!m.embed_between) {
		header_height += impl->embed_height;
	}

	// Move ports to appropriate locations
	int      i              = 0;
	gboolean last_was_input = FALSE;
	double   y              = 0.0;
	double   h              = 0.0;
	FOREACH_PORT(impl->ports, pi) {
		GanvPort* const p     = (*pi);
		GanvBox*  const pbox  = GANV_BOX(p);
		GanvNode* const pnode = GANV_NODE(p);
		h = ganv_box_get_height(pbox);

		if (p->impl->is_input) {
			y = header_height + 2.0 + (i * (h + 2.0));
			++i;
			ganv_node_move_to(pnode, 0.0, y);
			ganv_box_set_width(pbox, m.input_width);
			last_was_input = TRUE;

			ganv_canvas_for_each_edge_to(
				canvas, pnode,
				(GanvEdgeFunc)ganv_edge_update_location, NULL);
		} else {
			if (!m.horiz || !last_was_input) {
				y = header_height + 2.0 + (i * (h + 2.0));
				++i;
			}
			ganv_node_move_to(pnode, m.width - m.output_width, y);
			ganv_box_set_width(pbox, m.output_width);
			last_was_input = FALSE;

			ganv_canvas_for_each_edge_from(
				canvas, pnode,
				(GanvEdgeFunc)ganv_edge_update_location, NULL);
		}
	}

	if (impl->ports->len == 0) {
		h += header_height;
	}

	double height = y + h + 4.0;
	if (impl->embed_item && m.embed_between)
		height = MAX(height, impl->embed_height + header_height + 2.0);

	ganv_box_set_height(GANV_BOX(module), height);

	place_title(module, GANV_DIRECTION_RIGHT);
}

static void
resize_down(GanvModule* module)
{
	GanvCanvas*     canvas = GANV_CANVAS(GANV_ITEM(module)->canvas);
	GanvModuleImpl* impl   = module->impl;

	Metrics m;
	measure(module, &m);

	double title_w, title_h;
	title_size(module, &title_w, &title_h);

	static const double PAD = 2.0;

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
		if (p->impl->is_input) {
			in_x = PAD + (in_count++ * (port_breadth + PAD));
			ganv_node_move_to(pnode, in_x, 0);
			ganv_canvas_for_each_edge_to(
				canvas, pnode,
				(GanvEdgeFunc)ganv_edge_update_location, NULL);
		} else {
			out_x = PAD + (out_count++ * (port_breadth + PAD));
			ganv_node_move_to(pnode, out_x, height - ganv_box_get_height(pbox));
			ganv_canvas_for_each_edge_from(
				canvas, pnode,
				(GanvEdgeFunc)ganv_edge_update_location, NULL);
		}
	}

	ganv_box_set_height(GANV_BOX(module), height);
	ganv_box_set_width(GANV_BOX(module),
	                   MAX(title_w, MAX(in_x, out_x) + port_breadth) + PAD);

	place_title(module, GANV_DIRECTION_DOWN);
}

static void
measure_ports(GanvModule* module)
{
	GanvModuleImpl* impl = module->impl;

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
layout(GanvNode* self)
{
	GanvModule*     module = GANV_MODULE(self);
	GanvModuleImpl* impl   = module->impl;
	GanvNode*       node   = GANV_NODE(self);
	GanvCanvas*     canvas = GANV_CANVAS(GANV_ITEM(module)->canvas);

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

	switch (canvas->direction) {
	case GANV_DIRECTION_RIGHT:
		resize_right(module);
		break;
	case GANV_DIRECTION_DOWN:
		resize_down(module);
		break;
	}

	impl->must_resize = FALSE;
}

static void
ganv_module_resize(GanvNode* self)
{
	layout(self);

	if (parent_class->parent_class.resize) {
		parent_class->parent_class.resize(self);
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
	GanvCanvas*     canvas = GANV_CANVAS(GANV_ITEM(module)->canvas);
	GanvModuleImpl* impl   = module->impl;

	const double width = ganv_port_get_natural_width(port);
	if (port->impl->is_input && width > impl->widest_input) {
		impl->widest_input = width;
	} else if (!port->impl->is_input && width > impl->widest_output) {
		impl->widest_output = width;
	}

	impl->must_resize = TRUE;

	g_ptr_array_add(impl->ports, port);
	place_title(module, canvas->direction);
	ganv_item_request_update(GANV_ITEM(module));
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

		module->impl->must_resize = TRUE;
		ganv_item_request_update(GANV_ITEM(module));
	} else {
		fprintf(stderr, "Failed to find port to remove\n");
	}
}

static void
ganv_module_add(GanvItem* item, GanvItem* child)
{
	if (GANV_IS_PORT(child)) {
		ganv_module_add_port(GANV_MODULE(item), GANV_PORT(child));
	} else {
		fprintf(stderr, "warning: Non-port item added to module.\n");
	}
}

static void
ganv_module_remove(GanvItem* item, GanvItem* child)
{
	if (GANV_IS_PORT(child)) {
		ganv_module_remove_port(GANV_MODULE(item), GANV_PORT(child));
	} else {
		fprintf(stderr, "warning: Non-port item removed from module.\n");
	}
}

static void
ganv_module_update(GanvItem* item, int flags)
{
	GanvNode*   node   = GANV_NODE(item);
	GanvModule* module = GANV_MODULE(item);

	if (module->impl->must_resize) {
		layout(node);
	}

	GanvItemClass* item_class = GANV_ITEM_CLASS(parent_class);
	item_class->update(item, flags);

	FOREACH_PORT(module->impl->ports, p) {
		ganv_item_invoke_update(GANV_ITEM(*p), flags);
	}

	if (module->impl->embed_item) {
		ganv_item_invoke_update(GANV_ITEM(module->impl->embed_item), flags);
	}
}

static void
ganv_module_draw(GanvItem* item,
                 cairo_t* cr,
                 int cx, int cy,
                 int width, int height)
{
	GanvNode*   node   = GANV_NODE(item);
	GanvModule* module = GANV_MODULE(item);

	// Draw box
	if (GANV_ITEM_CLASS(parent_class)->draw) {
		(*GANV_ITEM_CLASS(parent_class)->draw)(item, cr, cx, cy, width, height);
	}

	// Draw label
	if (node->impl->label) {
		GanvItem* label_item = GANV_ITEM(node->impl->label);
		GANV_ITEM_GET_CLASS(label_item)->draw(label_item, cr, cx, cy, width, height);
	}

	// Draw ports
	FOREACH_PORT(module->impl->ports, p) {
		GANV_ITEM_GET_CLASS(GANV_ITEM(*p))->draw(
			GANV_ITEM(*p), cr, cx, cy, width, height);
	}

	// Draw embed item
	if (module->impl->embed_item) {
		GANV_ITEM_GET_CLASS(module->impl->embed_item)->draw(
			module->impl->embed_item, cr, cx, cy, width, height);
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
ganv_module_point(GanvItem* item,
                  double x, double y,
                  int cx, int cy,
                  GanvItem** actual_item)
{
	GanvModule* module = GANV_MODULE(item);

	double d = GANV_ITEM_CLASS(parent_class)->point(
		item, x, y, cx, cy, actual_item);

	if (!*actual_item) {
		// Point is not inside module at all, no point in checking children
		return d;
	}

	FOREACH_PORT(module->impl->ports, p) {
		GanvItem* const port = GANV_ITEM(*p);

		*actual_item = NULL;
		d = GANV_ITEM_GET_CLASS(port)->point(
			port,
			x - port->x, y - port->y,
			cx, cy,
			actual_item);

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

	g_type_class_add_private(klass, sizeof(GanvModuleImpl));

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
	GanvCanvas* canvas = GANV_CANVAS(GANV_ITEM(module)->canvas);

	return ganv_canvas_get_font_size(canvas) * 1.1;
}

static void
on_embed_size_request(GtkWidget*      widget,
                      GtkRequisition* r,
                      void*           user_data)
{
	GanvModule*     module = GANV_MODULE(user_data);
	GanvModuleImpl* impl   = module->impl;
	if (impl->embed_width == r->width && impl->embed_height == r->height) {
		return;
	}

	impl->embed_width  = r->width;
	impl->embed_height = r->height;

	impl->must_resize = TRUE;

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
	GanvModuleImpl* impl = module->impl;

	if (impl->embed_item) {
		gtk_object_destroy(GTK_OBJECT(impl->embed_item));
		impl->embed_item = NULL;
	}

	if (!widget) {
		impl->embed_width  = 0;
		impl->embed_height = 0;
		impl->must_resize  = TRUE;
		return;
	}

	double title_w, title_h;
	title_size(module, &title_w, &title_h);

	const double y = 4.0 + title_h;
	impl->embed_item = ganv_item_new(
		GANV_ITEM(module),
		ganv_widget_get_type(),
		"x", 2.0,
		"y", y,
		"widget", widget,
		NULL);

	gtk_widget_show_all(widget);

	GtkRequisition r;
	gtk_widget_size_request(widget, &r);
	on_embed_size_request(widget, &r, module);

	ganv_item_show(impl->embed_item);

	g_signal_connect(widget, "size-request",
	                 G_CALLBACK(on_embed_size_request), module);

	layout(GANV_NODE(module));
	ganv_item_request_update(GANV_ITEM(module));
}

void
ganv_module_set_direction(GanvModule*   module,
                          GanvDirection direction)
{
	FOREACH_PORT(module->impl->ports, p) {
		ganv_port_set_direction(*p, direction);
	}
	module->impl->must_resize = TRUE;
	ganv_module_resize(GANV_NODE(module));
}

void
ganv_module_for_each_port(GanvModule*  module,
                          GanvPortFunc f,
                          void*        data)
{
	GanvModuleImpl* impl = module->impl;
	const int       len  = impl->ports->len;
	GanvPort**      copy = (GanvPort**)malloc(sizeof(GanvPort*) * len);
	memcpy(copy, impl->ports->pdata, sizeof(GanvPort*) * len);

	for (int i = 0; i < len; ++i) {
		f(copy[i], data);
	}

	free(copy);
}
