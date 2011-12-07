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

#include <string.h>

#include "ganv/canvas.h"
#include "ganv/module.h"
#include "ganv/port.h"

#include "./color.h"
#include "./boilerplate.h"
#include "./gettext.h"

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
	PROP_SHOW_PORT_LABELS
};

static void
ganv_module_init(GanvModule* module)
{
	GANV_NODE(module)->can_head = FALSE;
	GANV_NODE(module)->can_tail = FALSE;

	module->ports = g_ptr_array_new();
	module->icon_box          = NULL;
	module->embed_item        = NULL;
	module->embed_width       = 0;
	module->embed_height      = 0;
	module->widest_input      = 0.0;
	module->widest_output     = 0.0;
	module->show_port_labels  = FALSE;
	module->must_resize       = TRUE;
	module->port_size_changed = FALSE;
}

static void
ganv_module_destroy(GtkObject* object)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(GANV_IS_MODULE(object));

	g_ptr_array_free(GANV_MODULE(object)->ports, TRUE);

	GanvModule* module = GANV_MODULE(object);
	FOREACH_PORT(module->ports, p) {
		gtk_object_destroy(GTK_OBJECT(*p));
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

	GanvModule* module = GANV_MODULE(object);

	switch (prop_id) {
	case PROP_SHOW_PORT_LABELS: {
		const gboolean tmp = g_value_get_boolean(value);
		if (module->show_port_labels != tmp) {
			module->show_port_labels  = tmp;
			module->port_size_changed = TRUE;
			module->must_resize       = TRUE;
			/* FIXME
			   FOREACH_PORT_CONST(gobj()->ports, p) {
			   (*p)->show_label(b);
			   }*/
			gnome_canvas_item_request_update(GNOME_CANVAS_ITEM(object));
		}
		break;
	}
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

	GanvModule* module = GANV_MODULE(object);

	switch (prop_id) {
		GET_CASE(SHOW_PORT_LABELS, boolean, module->show_port_labels);
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
	if (module->box.node.label) {
		g_object_get(G_OBJECT(module->box.node.label),
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

	GanvCanvas* canvas = GANV_CANVAS(GNOME_CANVAS_ITEM(module)->canvas);
	GanvText* canvas_title = module->box.node.label;

	GanvDirection direction = canvas->direction;

	if (direction == GANV_VERTICAL) {
		static const double PAD = 2.0;

		double contents_width = PAD;
		if (canvas_title) {
			contents_width += title_w;
		}
		if (module->icon_box) {
			contents_width += MODULE_ICON_SIZE + PAD;
		}

		m->embed_x      = 0;
		m->input_width  = ganv_module_get_empty_port_breadth(module);
		m->output_width = ganv_module_get_empty_port_breadth(module);

		const double ports_width = PAD + ((m->input_width + PAD) * module->ports->len);

		m->width = MAX(contents_width, ports_width);
		m->width = MAX(m->width, module->embed_width);
		return;
	}

	// The amount of space between a port edge and the module edge (on the
	// side that the port isn't right on the edge).
	const double hor_pad = (canvas_title ? 10.0 : 20.0);

	m->width = (canvas_title)
		? title_w + 10.0
		: 1.0;

	if (module->icon_box)
		m->width += MODULE_ICON_SIZE + 2;

	// Title is wide, put inputs and outputs beside each other
	m->horiz = (module->widest_input + module->widest_output + 10.0
	            < MAX(m->width, module->embed_width));

	// Fit ports to module (or vice-versa)
	m->input_width  = module->widest_input;
	m->output_width = module->widest_output;
	double expand_w = (m->horiz ? (m->width / 2.0) : m->width) - hor_pad;
	if (module->show_port_labels && !module->embed_item) {
		m->input_width  = MAX(module->widest_input,  expand_w);
		m->output_width = MAX(module->widest_output, expand_w);
	}

	const double widest = MAX(m->input_width, m->output_width);

	if (module->embed_item) {
		double above_w   = MAX(m->width, widest + hor_pad);
		double between_w = MAX(m->width,
		                       (m->input_width
		                        + m->output_width
		                        + module->embed_width));

		above_w = MAX(above_w, module->embed_width);

		// Decide where to place embedded widget if necessary)
		if (module->embed_width < module->embed_height * 2.0) {
			m->embed_between = TRUE;
			m->width         = between_w;
			m->embed_x       = m->input_width;
		} else {
			m->width   = above_w;
			m->embed_x = 2.0;
		}
	}

	if (!canvas_title && (module->widest_input == 0.0
	                      || module->widest_output == 0.0)) {
		m->width += 10.0;
	}

	m->width += 4.0;
	m->width = MAX(m->width, widest + hor_pad);
}

static void
place_title(GanvModule* module, GanvDirection dir)
{
	GanvBox* box = GANV_BOX(module);

	double title_w, title_h;
	title_size(module, &title_w, &title_h);

	GanvText* canvas_title = module->box.node.label;

	if (!canvas_title) {
		return;
	} else if (dir == GANV_HORIZONTAL) {
		if (module->icon_box) {
			gnome_canvas_item_set(GNOME_CANVAS_ITEM(canvas_title),
			                      "x", MODULE_ICON_SIZE + 1.0,
			                      NULL);
		} else {
			gnome_canvas_item_set(GNOME_CANVAS_ITEM(canvas_title),
			                      "x", ((ganv_box_get_width(box) / 2.0)
			                            - (title_w / 2.0)),
			                      NULL);
		}
	} else {
		gnome_canvas_item_set(GNOME_CANVAS_ITEM(canvas_title),
		                      "x", ((ganv_box_get_width(box) / 2.0)
		                            - (title_w / 2.0)),
		                      "y", ganv_module_get_empty_port_depth(module) + 2.0,
		                      NULL);
	}
}

static void
resize_horiz(GanvModule* module)
{
	GanvCanvas* canvas = GANV_CANVAS(GNOME_CANVAS_ITEM(module)->canvas);

	Metrics m;
	measure(module, &m);

	double title_w, title_h;
	title_size(module, &title_w, &title_h);

	// Basic height contains title, icon
	double header_height = 2.0 + title_h;

	double height = header_height;

	if (module->embed_item) {
		gnome_canvas_item_set(module->embed_item,
		                      "x", (double)m.embed_x,
		                      NULL);
	}

	// Actually set width and height
	ganv_box_set_width(GANV_BOX(module), m.width);

	// Offset ports below embedded widget
	if (!m.embed_between) {
		header_height += module->embed_height;
	}

	// Move ports to appropriate locations
	int      i              = 0;
	gboolean last_was_input = FALSE;
	double   y              = 0.0;
	double   h              = 0.0;
	FOREACH_PORT(module->ports, pi) {
		GanvPort* const p     = (*pi);
		GanvBox*  const pbox  = GANV_BOX(p);
		GanvNode* const pnode = GANV_NODE(p);
		h = ganv_box_get_height(pbox);

		if (p->is_input) {
			y = header_height + (i * (h + 1.0));
			++i;
			ganv_box_set_width(pbox, m.input_width);
			ganv_node_move_to(pnode, 0.0, y);
			last_was_input = TRUE;

			ganv_canvas_for_each_edge_to(
				canvas, pnode, ganv_edge_update_location);
		} else {
			if (!m.horiz || !last_was_input) {
				y = header_height + (i * (h + 1.0));
				++i;
			}
			ganv_box_set_width(pbox, m.output_width);
			ganv_node_move_to(pnode,
			                  m.width - ganv_box_get_width(pbox),
			                  y);
			last_was_input = FALSE;

			ganv_canvas_for_each_edge_from(
				canvas, pnode, ganv_edge_update_location);
		}
	}

	if (module->ports->len == 0) {
		h += header_height;
	}

	height = y + h + 4.0;
	if (module->embed_item && m.embed_between)
		height = MAX(height, module->embed_height + header_height + 2.0);

	ganv_box_set_height(&module->box, height);

	place_title(module, GANV_HORIZONTAL);
}

static void
resize_vert(GanvModule* module)
{
	GanvCanvas* canvas = GANV_CANVAS(GNOME_CANVAS_ITEM(module)->canvas);

	Metrics m;
	measure(module, &m);

	double title_w, title_h;
	title_size(module, &title_w, &title_h);

	static const double PAD = 2.0;

	const double port_depth   = ganv_module_get_empty_port_depth(module);
	const double port_breadth = ganv_module_get_empty_port_breadth(module);

	if (module->embed_item) {
		gnome_canvas_item_set(module->embed_item,
		                      "x", (double)m.embed_x,
		                      "y", port_depth + title_h,
		                      NULL);
	}

	const double height = PAD + title_h
		+ module->embed_height + (port_depth * 2.0);

	// Move ports to appropriate locations
	int      i              = 0;
	gboolean last_was_input = FALSE;
	double   x              = 0.0;
	FOREACH_PORT(module->ports, pi) {
		GanvPort* const p     = (*pi);
		GanvBox*  const pbox  = GANV_BOX(p);
		GanvNode* const pnode = GANV_NODE(p);
		ganv_box_set_width(pbox, port_breadth);
		ganv_box_set_height(pbox, port_depth);
		if (p->is_input) {
			x = PAD + (i * (port_breadth + PAD));
			++i;
			ganv_node_move_to(pnode, x, 0);
			last_was_input = TRUE;

			ganv_canvas_for_each_edge_to(canvas, pnode,
			                             ganv_edge_update_location);
		} else {
			if (!last_was_input) {
				x = PAD + (i * (port_breadth + PAD));
				++i;
			}
			ganv_node_move_to(pnode,
			                  x,
			                  height - ganv_box_get_height(pbox));
			last_was_input = FALSE;

			ganv_canvas_for_each_edge_from(canvas, pnode,
			                               ganv_edge_update_location);
		}
	}

	ganv_box_set_width(GANV_BOX(module), m.width);
	ganv_box_set_height(GANV_BOX(module), height);

	place_title(module, GANV_VERTICAL);
}

static void
measure_ports(GanvModule* module)
{
	module->widest_input  = 0.0;
	module->widest_output = 0.0;
	FOREACH_PORT_CONST(module->ports, pi) {
		const GanvPort* const p = (*pi);
		const double                w = ganv_port_get_natural_width(p);
		if (p->is_input) {
			if (w > module->widest_input) {
				module->widest_input = w;
			}
		} else {
			if (w > module->widest_output) {
				module->widest_output = w;
			}
		}
	}
}

static void
layout(GanvNode* self)
{
	GanvModule* module = GANV_MODULE(self);
	GanvNode*   node   = GANV_NODE(self);
	GanvCanvas* canvas = GANV_CANVAS(GNOME_CANVAS_ITEM(module)->canvas);

	double label_w = 0.0;
	double label_h = 0.0;
	if (node->label) {
		g_object_get(node->label,
		             "width", &label_w,
		             "height", &label_h,
		             NULL);
	}

	ganv_box_set_width(&module->box, label_w + (MODULE_LABEL_PAD * 2.0));
	ganv_box_set_height(&module->box, label_h);

	if (module->port_size_changed) {
		measure_ports(module);
		module->port_size_changed = FALSE;
	}

	switch (canvas->direction) {
	case GANV_HORIZONTAL:
		resize_horiz(module);
		break;
	case GANV_VERTICAL:
		resize_vert(module);
		break;
	}

	module->must_resize = FALSE;
}

static void
ganv_module_resize(GanvNode* self)
{
	GanvModule* module = GANV_MODULE(self);

	if (module->must_resize) {
		layout(self);
	}

	if (parent_class->parent_class.resize) {
		parent_class->parent_class.resize(self);
	}
}

static void
ganv_module_update(GnomeCanvasItem* item,
                   double*          affine,
                   ArtSVP*          clip_path,
                   int              flags)
{
	GanvNode*   node   = GANV_NODE(item);
	GanvModule* module = GANV_MODULE(item);
	if (module->must_resize) {
		layout(node);
	}

	GnomeCanvasItemClass* item_class = GNOME_CANVAS_ITEM_CLASS(parent_class);
	item_class->update(item, affine, clip_path, flags);
}

static void
ganv_module_move_to(GanvNode* node,
                    double    x,
                    double    y)
{
	GanvModule* module = GANV_MODULE(node);
	GANV_NODE_CLASS(parent_class)->move_to(node, x, y);
	FOREACH_PORT(module->ports, p) {
		ganv_node_move(GANV_NODE(*p), 0.0, 0.0);
	}
}

static void
ganv_module_move(GanvNode* node,
                 double    dx,
                 double    dy)
{
	GanvModule* module = GANV_MODULE(node);
	GANV_NODE_CLASS(parent_class)->move(node, dx, dy);
	FOREACH_PORT(module->ports, p) {
		ganv_node_move(GANV_NODE(*p), 0.0, 0.0);
	}
}

static void
ganv_module_class_init(GanvModuleClass* class)
{
	GObjectClass*         gobject_class = (GObjectClass*)class;
	GtkObjectClass*       object_class  = (GtkObjectClass*)class;
	GnomeCanvasItemClass* item_class    = (GnomeCanvasItemClass*)class;
	GanvNodeClass*  node_class    = (GanvNodeClass*)class;

	parent_class = GANV_BOX_CLASS(g_type_class_peek_parent(class));

	gobject_class->set_property = ganv_module_set_property;
	gobject_class->get_property = ganv_module_get_property;

	object_class->destroy = ganv_module_destroy;

	item_class->update = ganv_module_update;

	node_class->move    = ganv_module_move;
	node_class->move_to = ganv_module_move_to;
	node_class->resize  = ganv_module_resize;
}

GanvModule*
ganv_module_new(GanvCanvas* canvas,
                const char* first_prop_name, ...)
{
	GanvModule* module = GANV_MODULE(
		g_object_new(ganv_module_get_type(), NULL));

	GnomeCanvasItem* item = GNOME_CANVAS_ITEM(module);
	va_list args;
	va_start(args, first_prop_name);
	gnome_canvas_item_construct(item,
	                            gnome_canvas_root(GNOME_CANVAS(canvas)),
	                            first_prop_name, args);
	va_end(args);

	ganv_canvas_add_node(canvas, GANV_NODE(module));
	return module;
}

void
ganv_module_add_port(GanvModule* module,
                     GanvPort*   port)
{
	const double width = ganv_port_get_natural_width(port);
	if (port->is_input && width > module->widest_input) {
		module->widest_input = width;
		module->must_resize  = TRUE;
	} else if (!port->is_input && width > module->widest_output) {
		module->widest_output = width;
		module->must_resize   = TRUE;
	}

#if 0

	double port_x, port_y;

	// Place vertically
	if (canvas()->direction() == Canvas::HORIZONTAL) {
		if (gobj()->ports->len != 0) {
			const Port* const last_port = *back();
			port_y = last_port->get_y() + last_port->get_height() + 1;
		} else {
			port_y = 2.0 + title_height();
		}
	} else {
		if (p->is_input()) {
			port_y = 0.0;
		} else {
			port_y = get_height() - get_empty_port_depth();
		}
	}

	// Place horizontally
	Metrics m;
	calculate_metrics(m);

	set_width(m.width);
	if (p->is_input()) {
		p->set_width(m.input_width);
		port_x = 0;
	} else {
		p->set_width(m.output_width);
		port_x = m.width - p->get_width();
	}

	p->move_to(port_x, port_y);

	g_ptr_array_add(gobj()->ports, p->gobj());
	if (canvas()->direction() == Canvas::HORIZONTAL) {
		set_height(p->get_y() + p->get_height() + 1);
	}

	place_title();
#endif
	module->must_resize = TRUE;

	g_ptr_array_add(module->ports, port);
	//if (canvas()->direction() == Canvas::HORIZONTAL) {
	//	set_height(p->get_y() + p->get_height() + 1);
	//}

	GanvCanvas* canvas = GANV_CANVAS(
		GNOME_CANVAS_ITEM(module)->canvas);

	place_title(module, canvas->direction);

}

void
ganv_module_remove_port(GanvModule* module,
                        GanvPort*   port)
{
#if 0
	gboolean removed = g_ptr_array_remove(gobj()->ports, port->gobj());
	if (removed) {
		// Find new widest input or output, if necessary
		if (port->is_input() && port->get_width() >= _widest_input) {
			_widest_input = 0;
			FOREACH_PORT_CONST(gobj()->ports, i) {
				const Port* const p = (*i);
				if (p->is_input() && p->get_width() >= _widest_input) {
					_widest_input = p->get_width();
				}
			}
		} else if (port->is_output() && port->get_width() >= _widest_output) {
			_widest_output = 0;
			FOREACH_PORT_CONST(gobj()->ports, i) {
				const Port* const p = (*i);
				if (p->is_output() && p->get_width() >= _widest_output) {
					_widest_output = p->get_width();
				}
			}
		}

		_must_resize = true;
		gnome_canvas_item_request_update(GNOME_CANVAS_ITEM(_gobj));
	} else {
		std::cerr << "Failed to find port to remove" << std::endl;
	}
#endif
}

double
ganv_module_get_empty_port_breadth(const GanvModule* module)
{
	return ganv_module_get_empty_port_depth(module) * 2.0;
}

double
ganv_module_get_empty_port_depth(const GanvModule* module)
{
	GanvCanvas* canvas = GANV_CANVAS(
		GNOME_CANVAS_ITEM(module)->canvas);

	return ganv_canvas_get_font_size(canvas);
}	

void
ganv_module_set_icon(GanvModule* module,
                     GdkPixbuf*        icon)
{
	if (module->icon_box) {
		gtk_object_destroy(GTK_OBJECT(module->icon_box));
		module->icon_box = NULL;
	}

	if (icon) {
		module->icon_box = gnome_canvas_item_new(
			GNOME_CANVAS_GROUP(module),
			gnome_canvas_pixbuf_get_type(),
			"x", 8.0,
			"y", 10.0,
			"pixbuf", icon,
			NULL);

		const double icon_w = gdk_pixbuf_get_width(icon);
		const double icon_h = gdk_pixbuf_get_height(icon);
		const double scale  = MODULE_ICON_SIZE / ((icon_w > icon_h)
		                                          ? icon_w : icon_h);
		const double scale_trans[6] = {
			scale, 0.0, 0.0,
			scale, 0.0, 0.0
		};

		gnome_canvas_item_affine_relative(module->icon_box, scale_trans);
		gnome_canvas_item_raise_to_top(module->icon_box);
		gnome_canvas_item_show(module->icon_box);
	}
	module->must_resize = TRUE;
}

static void
on_embed_size_request(GtkWidget*      widget,
                      GtkRequisition* r,
                      void*           user_data)
{
	GanvModule* module = GANV_MODULE(user_data);
	if (module->embed_width == r->width && module->embed_height == r->height) {
		return;
	}

	module->embed_width  = r->width;
	module->embed_height = r->height;

	module->must_resize = TRUE;

	GtkAllocation allocation;
	allocation.width = r->width;
	allocation.height = r->width;

	gtk_widget_size_allocate(widget, &allocation);
	gnome_canvas_item_set(module->embed_item,
	                      "width", (double)r->width,
	                      "height", (double)r->height,
	                      NULL);
}

void
ganv_module_embed(GanvModule* module,
                  GtkWidget*  widget)
{
	if (module->embed_item) {
		gtk_object_destroy(GTK_OBJECT(module->embed_item));
		module->embed_item = NULL;
	}

	if (!widget) {
		module->embed_width  = 0;
		module->embed_height = 0;
		module->must_resize  = TRUE;
		return;
	}

	double title_w, title_h;
	title_size(module, &title_w, &title_h);

	const double y = 4.0 + title_h;
	module->embed_item = gnome_canvas_item_new(
		GNOME_CANVAS_GROUP(module),
		gnome_canvas_widget_get_type(),
		"x", 2.0,
		"y", y,
		"widget", widget,
		NULL);

	gtk_widget_show_all(widget);

	GtkRequisition r;
	gtk_widget_size_request(widget, &r);
	on_embed_size_request(widget, &r, module);

	gnome_canvas_item_show(module->embed_item);
	gnome_canvas_item_raise_to_top(module->embed_item);

	g_signal_connect(widget, "size-request",
	                 G_CALLBACK(on_embed_size_request), module);

	layout(GANV_NODE(module));
	gnome_canvas_item_request_update(GNOME_CANVAS_ITEM(module));
}

void
ganv_module_for_each_port(GanvModule*      module,
                          GanvPortFunction f,
                          void*            data)
{
	const int len = module->ports->len;
	GanvPort** copy = (GanvPort**)malloc(sizeof(GanvPort*) * len);
	memcpy(copy, module->ports->pdata, sizeof(GanvPort*) * len);

	for (int i = 0; i < len; ++i) {
		f(copy[i], data);
	}

	free(copy);
}

