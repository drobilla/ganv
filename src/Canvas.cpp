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

#define _POSIX_C_SOURCE 200809L // strdup

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <list>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <set>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtkstyle.h>

#include <gtkmm/widget.h>

#include "ganv_config.h"
#include "ganv/Canvas.hpp"
#include "ganv/Edge.hpp"
#include "ganv/Circle.hpp"
#include "ganv/Module.hpp"
#include "ganv/Port.hpp"
#include "ganv/box.h"
#include "ganv/canvas.h"
#include "ganv/edge.h"
#include "ganv/node.h"

#include "./color.h"
#include "./ganv-marshal.h"
#include "./ganv-private.h"

#if defined(HAVE_AGRAPH_2_20) || defined(HAVE_AGRAPH_2_30)
#    include <gvc.h>
#endif
#ifdef GANV_FDGL
#    include "./fdgl.hpp"
#endif

static guint signal_connect;
static guint signal_disconnect;

static GEnumValue dir_values[3];

using std::cerr;
using std::endl;
using std::list;
using std::string;
using std::vector;

typedef std::set<GanvNode*> Items;

#define FOREACH_ITEM(items, i) \
	for (Items::const_iterator i = items.begin(); i != items.end(); ++i)

#define FOREACH_ITEM_MUT(items, i) \
	for (Items::iterator i = items.begin(); i != items.end(); ++i)

#define FOREACH_EDGE(edges, i) \
	for (GanvCanvasImpl::Edges::const_iterator i = edges.begin(); \
	     i != edges.end(); \
	     ++i)

#define FOREACH_EDGE_MUT(edges, i) \
	for (GanvCanvasImpl::Edges::iterator i = edges.begin(); \
	     i != edges.end(); \
	     ++i)

#define FOREACH_SELECTED_EDGE(edges, i) \
	for (GanvCanvasImpl::SelectedEdges::const_iterator i = edges.begin(); \
	     i != edges.end(); \
	     ++i)

#define FOREACH_SELECTED_PORT(p) \
	for (SelectedPorts::iterator p = _selected_ports.begin(); \
	     p != _selected_ports.end(); ++p)

#if defined(HAVE_AGRAPH_2_20) || defined(HAVE_AGRAPH_2_30)
class GVNodes : public std::map<GanvNode*, Agnode_t*> {
public:
	GVNodes() : gvc(0), G(0) {}

	void cleanup() {
		gvFreeLayout(gvc, G);
		agclose (G);
		gvc = 0;
		G = 0;
	}

	GVC_t*    gvc;
	Agraph_t* G;
};
#endif

static const uint32_t SELECT_RECT_FILL_COLOUR   = 0x0E2425FF;
static const uint32_t SELECT_RECT_BORDER_COLOUR = 0x2E4445FF;

/** Order edges by (tail, head) */
struct TailHeadOrder {
	inline bool operator()(const GanvEdge* a, const GanvEdge* b) const {
		return ((a->impl->tail < b->impl->tail)
		        || (a->impl->tail == b->impl->tail
		            && a->impl->head < b->impl->head));
	}
};

/** Order edges by (head, tail) */
struct HeadTailOrder {
	inline bool operator()(const GanvEdge* a, const GanvEdge* b) const {
		return ((a->impl->head < b->impl->head)
		        || (a->impl->head == b->impl->head
		            && a->impl->tail < b->impl->tail));
	}
};

struct GanvCanvasImpl {
	GanvCanvasImpl(GanvCanvas* gobj)
		: _gcanvas(gobj)
		, _layout(GTK_LAYOUT(_gcanvas))
		, _connect_port(NULL)
		, _last_selected_port(NULL)
		, _drag_edge(NULL)
		, _drag_node(NULL)
		, _select_rect(NULL)
		, _select_start_x(0.0)
		, _select_start_y(0.0)
		, _zoom(1.0)
		, _font_size(0.0)
		, _drag_state(NOT_DRAGGING)
		, _layout_idle_id(0)
	{
		_wrapper_key = g_quark_from_string("ganvmm");
		_move_cursor = gdk_cursor_new(GDK_FLEUR);

		g_signal_connect(G_OBJECT(ganv_canvas_base_root(GANV_CANVAS_BASE(_gcanvas))),
		                 "event", G_CALLBACK(on_canvas_event), this);
	}

	~GanvCanvasImpl()
	{
		gdk_cursor_unref(_move_cursor);
	}

	static gboolean
	on_canvas_event(GanvItem* canvasitem,
	                GdkEvent* ev,
	                void*     impl)
	{
		return ((GanvCanvasImpl*)impl)->on_event(ev);
	}

#ifdef GANV_FDGL
	static gboolean on_layout_timeout(gpointer impl) {
		return ((GanvCanvasImpl*)impl)->layout_iteration();
	}

	static void on_layout_done(gpointer impl) {
		((GanvCanvasImpl*)impl)->_layout_idle_id = 0;
	}

	gboolean layout_iteration();
	gboolean layout_calculate(double dur, bool update);

#endif

	GanvItem* root() {
		return ganv_canvas_base_root(GANV_CANVAS_BASE(_gcanvas));
	}

	void resize(double width, double height);
	void contents_changed();

	void set_zoom_and_font_size(double zoom, double points);

	void for_each_node(GanvNodeFunc f, void* data);
	void for_each_edge_from(const GanvNode* tail, GanvEdgeFunc f, void* data);
	void for_each_edge_to(const GanvNode* head, GanvEdgeFunc f, void* data);
	void for_each_edge_on(const GanvNode* node, GanvEdgeFunc f, void* data);

	void add_item(GanvNode* i);
	bool remove_item(GanvNode* i);

	void select_edge(GanvEdge* edge);
	void unselect_edge(GanvEdge* edge);
	void select_item(GanvNode* item);
	void unselect_item(GanvNode* item);
	void unselect_ports();
	void clear_selection();

	void move_selected_items(double dx, double dy);
	void selection_move_finished();

#if defined(HAVE_AGRAPH_2_20) || defined(HAVE_AGRAPH_2_30)
	GVNodes layout_dot(const std::string& filename);
#endif

	void remove_edge(GanvEdge* c);
	bool are_connected(const GanvNode* tail,
	                   const GanvNode* head) const;
	GanvEdge*
	get_edge_between(const GanvNode* tail,
	                 const GanvNode* head) const;

	typedef std::set<GanvEdge*, TailHeadOrder> Edges;
	typedef std::set<GanvEdge*, HeadTailOrder> DstEdges;
	typedef std::set<GanvEdge*>                SelectedEdges;
	typedef std::set<GanvPort*>                SelectedPorts;

	Edges::const_iterator    first_edge_from(const GanvNode* src);
	DstEdges::const_iterator first_edge_to(const GanvNode* dst);

	void select_port(GanvPort* p, bool unique=false);
	void select_port_toggle(GanvPort* p, int mod_state);
	void unselect_port(GanvPort* p);
	void selection_joined_with(GanvPort* port);
	void join_selection();

	GanvNode* get_node_at(double x, double y);

	bool on_event(GdkEvent* event);

	bool scroll_drag_handler(GdkEvent* event);
	bool select_drag_handler(GdkEvent* event);
	bool connect_drag_handler(GdkEvent* event);
	void end_connect_drag();

	/*
	   Event handler for ports.

	   This must be implemented as a Canvas method since port event handling
	   depends on shared data (for selection and connecting).  This function
	   should only be used by Port implementations.
	*/
	bool port_event(GdkEvent* event, GanvPort* port);

	void ports_joined(GanvPort* port1, GanvPort* port2);
	bool animate_selected();

	void move_contents_to_internal(double x, double y, double min_x, double min_y);

	GanvCanvas* _gcanvas;
	GtkLayout*  _layout;

	Items         _items;       ///< Items on this canvas
	Edges         _edges;       ///< Edges ordered (src, dst)
	DstEdges      _dst_edges;   ///< Edges ordered (dst, src)
	Items         _selected_items; ///< Currently selected items
	SelectedEdges _selected_edges; ///< Currently selected edges

	SelectedPorts _selected_ports; ///< Selected ports (hilited red)
	GanvPort*     _connect_port; ///< Port for which a edge is being made
	GanvPort*     _last_selected_port;
	GanvEdge*     _drag_edge;
	GanvNode*     _drag_node;

	GanvBox* _select_rect;     ///< Rectangle for drag selection
	double   _select_start_x;  ///< Selection drag start x coordinate
	double   _select_start_y;  ///< Selection drag start y coordinate

	double _zoom;       ///< Current zoom level
	double _font_size;  ///< Current font size in points

	enum DragState { NOT_DRAGGING, EDGE, SCROLL, SELECT };
	DragState      _drag_state;

	GQuark     _wrapper_key;
	GdkCursor* _move_cursor;
	guint      _layout_idle_id;
};

typedef struct {
	GanvItem      item;
	GanvEdgeImpl* impl;
	GanvEdgeImpl  impl_data;
} GanvEdgeKey;

static void
make_edge_search_key(GanvEdgeKey*    key,
                     const GanvNode* tail,
                     const GanvNode* head)
{
	memset(key, '\0', sizeof(GanvEdgeKey));
	key->impl = &key->impl_data;
	key->impl->tail = const_cast<GanvNode*>(tail);
	key->impl->head = const_cast<GanvNode*>(head);
}

GanvCanvasImpl::Edges::const_iterator
GanvCanvasImpl::first_edge_from(const GanvNode* tail)
{
	GanvEdgeKey key;
	make_edge_search_key(&key, tail, NULL);
	return _edges.lower_bound((GanvEdge*)&key);
}

GanvCanvasImpl::DstEdges::const_iterator
GanvCanvasImpl::first_edge_to(const GanvNode* head)
{
	GanvEdgeKey key;
	make_edge_search_key(&key, NULL, head);
	return _dst_edges.lower_bound((GanvEdge*)&key);
}

void
GanvCanvasImpl::select_edge(GanvEdge* edge)
{
	ganv_item_set(GANV_ITEM(edge), "selected", TRUE, NULL);
	_selected_edges.insert(edge);
}

void
GanvCanvasImpl::unselect_edge(GanvEdge* edge)
{
	ganv_item_set(GANV_ITEM(edge), "selected", FALSE, NULL);
	_selected_edges.erase(edge);
}

void
GanvCanvasImpl::move_selected_items(double dx, double dy)
{
	FOREACH_ITEM(_selected_items, i) {
		ganv_node_move(*i, dx, dy);
	}
}

void
GanvCanvasImpl::selection_move_finished()
{
	FOREACH_ITEM(_selected_items, i) {
		const double x = GANV_ITEM(*i)->x;
		const double y = GANV_ITEM(*i)->y;
		g_signal_emit(*i, signal_moved, 0, x, y, NULL);
	}
}

static void
select_if_tail_is_selected(GanvEdge* edge, void* data)
{
	GanvNode* tail = edge->impl->tail;
	gboolean  selected;
	g_object_get(tail, "selected", &selected, NULL);
	if (!selected && GANV_IS_PORT(tail)) {
		g_object_get(ganv_port_get_module(GANV_PORT(tail)),
		             "selected", &selected, NULL);
	}

	if (selected) {
		ganv_edge_select(edge);
	}
}

static void
select_if_head_is_selected(GanvEdge* edge, void* data)
{
	GanvNode* head = edge->impl->head;
	gboolean  selected;
	g_object_get(head, "selected", &selected, NULL);
	if (!selected && GANV_IS_PORT(head)) {
		g_object_get(ganv_port_get_module(GANV_PORT(head)),
		             "selected", &selected, NULL);
	}

	if (selected) {
		ganv_edge_select(edge);
	}
}

static void
select_edges(GanvPort* port, void* data)
{
	GanvCanvasImpl* impl = (GanvCanvasImpl*)data;
	if (port->impl->is_input) {
		impl->for_each_edge_to(GANV_NODE(port),
		                       select_if_tail_is_selected,
		                       NULL);
	} else {
		impl->for_each_edge_from(GANV_NODE(port),
		                         select_if_head_is_selected,
		                         NULL);
	}
}

void
GanvCanvasImpl::add_item(GanvNode* n)
{
	GanvItem* item = GANV_ITEM(n);
	if (item->parent == GANV_ITEM(root())) {
		_items.insert(n);
	}
}

/** Remove an item from the canvas, cutting all references.
 * Returns true if item was found (and removed).
 */
bool
GanvCanvasImpl::remove_item(GanvNode* item)
{
	bool ret = false;

	if (item == (GanvNode*)_connect_port) {
		if (_drag_state == EDGE) {
			ganv_item_ungrab(GANV_ITEM(root()), 0);
			end_connect_drag();
		}
		_connect_port = NULL;
	}

	// Remove from selection
	_selected_items.erase(item);

	// Remove children ports from selection if item is a module
	if (GANV_IS_MODULE(item)) {
		GanvModule* const module = GANV_MODULE(item);
		for (unsigned i = 0; i < ganv_module_num_ports(module); ++i) {
			unselect_port(ganv_module_get_port(module, i));
		}
	}

	// Remove from items
	_items.erase(item);

	return ret;
}

static void
select_if_ends_are_selected(GanvEdge* edge, void* data)
{
	if (ganv_node_is_selected(ganv_edge_get_tail(edge)) &&
	    ganv_node_is_selected(ganv_edge_get_head(edge))) {
		ganv_edge_select(edge);
	}
}
	    
void
GanvCanvasImpl::select_item(GanvNode* m)
{
	_selected_items.insert(m);

	// Select any connections to or from this node
	if (GANV_IS_MODULE(m)) {
		ganv_module_for_each_port(GANV_MODULE(m), select_edges, this);
	} else {
		for_each_edge_on(m, select_if_ends_are_selected, this);
	}

	g_object_set(m, "selected", TRUE, NULL);
}

static void
unselect_edges(GanvPort* port, void* data)
{
	GanvCanvasImpl* impl = (GanvCanvasImpl*)data;
	if (port->impl->is_input) {
		impl->for_each_edge_to(GANV_NODE(port),
		                       (GanvEdgeFunc)ganv_edge_unselect,
		                       NULL);
	} else {
		impl->for_each_edge_from(GANV_NODE(port),
		                         (GanvEdgeFunc)ganv_edge_unselect,
		                         NULL);
	}
}

void
GanvCanvasImpl::unselect_item(GanvNode* m)
{
	// Unselect any connections to or from this node
	if (GANV_IS_MODULE(m)) {
		ganv_module_for_each_port(GANV_MODULE(m), unselect_edges, this);
	} else {
		for_each_edge_on(m, (GanvEdgeFunc)ganv_edge_unselect, NULL);
	}

	// Unselect item
	_selected_items.erase(m);
	g_object_set(m, "selected", FALSE, NULL);
}

#if defined(HAVE_AGRAPH_2_20) || defined(HAVE_AGRAPH_2_30)
static void
gv_set(void* subject, const char* key, double value)
{
	std::ostringstream ss;
	ss << value;
	agsafeset(subject, (char*)key, (char*)ss.str().c_str(), (char*)"");
}

GVNodes
GanvCanvasImpl::layout_dot(const std::string& filename)
{
	GVNodes nodes;

	const double dpi = gdk_screen_get_resolution(gdk_screen_get_default());

	GVC_t* gvc = gvContext();

#ifndef HAVE_AGRAPH_2_30
#define agstrdup_html(g, str) agstrdup_html(str)
#define agedge(g, t, h, name, flag) agedge(g, t, h)
#define agnode(g, name, flag) agnode(g, name)
#define agattr(g, t, k, v) agraphattr(g, k, v)
	Agraph_t* G = agopen((char*)"g", AGDIGRAPH);
#else
	Agraph_t* G = agopen((char*)"g", Agdirected, NULL);
#endif

	agsafeset(G, (char*)"splines", (char*)"false", (char*)"");
	agsafeset(G, (char*)"compound", (char*)"true", (char*)"");
	agsafeset(G, (char*)"remincross", (char*)"true", (char*)"");
	agsafeset(G, (char*)"overlap", (char*)"scale", (char*)"");
	agsafeset(G, (char*)"nodesep", (char*)"0.05", (char*)"");
	gv_set(G, "fontsize", ganv_canvas_get_font_size(_gcanvas));
	gv_set(G, "dpi", dpi);

	nodes.gvc = gvc;
	nodes.G   = G;

	const bool flow_right = _gcanvas->direction;
	if (flow_right) {
		agattr(G, AGRAPH, (char*)"rankdir", (char*)"LR");
	} else {
		agattr(G, AGRAPH, (char*)"rankdir", (char*)"TD");
	}

	unsigned id = 0;
	std::ostringstream ss;
	FOREACH_ITEM(_items, i) {
		ss.str("");
		ss << "n" << id++;
		const std::string node_id = ss.str();
		
		Agnode_t* node = agnode(G, strdup(node_id.c_str()), true);
		nodes.insert(std::make_pair(*i, node));

		if (GANV_IS_MODULE(*i)) {
			GanvModule* const m = GANV_MODULE(*i);

			agsafeset(node, (char*)"shape", (char*)"plaintext", (char*)"");
			gv_set(node, "width", ganv_box_get_width(GANV_BOX(*i)) / dpi);
			gv_set(node, "height", ganv_box_get_height(GANV_BOX(*i)) / dpi);

			std::string inputs;   // Down flow
			std::string outputs;  // Down flow
			std::string ports;    // Right flow
			unsigned    n_inputs  = 0;
			unsigned    n_outputs = 0;
			for (size_t i = 0; i < ganv_module_num_ports(m); ++i) {
				GanvPort* port = ganv_module_get_port(m, i);
				ss.str("");
				ss << port;

				if (port->impl->is_input) {
					++n_inputs;
				} else {
					++n_outputs;
				}

				std::string cell = std::string("<TD PORT=\"") + ss.str() + "\"";

				cell += " FIXEDSIZE=\"TRUE\"";
				ss.str("");
				ss << ganv_box_get_width(GANV_BOX(port));// / dpp * 1.3333333;
				cell += " WIDTH=\"" + ss.str() + "\"";

				ss.str("");
				ss << ganv_box_get_height(GANV_BOX(port));// / dpp * 1.333333;
				cell += " HEIGHT=\"" + ss.str() + "\"";
			
				cell += ">";
				const char* label = ganv_node_get_label(GANV_NODE(port));
				if (label && flow_right) {
					cell += label;
				}
				cell += "</TD>";

				if (flow_right) {
					ports += "<TR>" + cell + "</TR>";
				} else if (port->impl->is_input) {
					inputs += cell;
				} else {
					outputs += cell;
				}
				
				nodes.insert(std::make_pair(GANV_NODE(port), node));
			}

			const unsigned n_cols = std::max(n_inputs, n_outputs);
			
			std::string html = "<TABLE CELLPADDING=\"0\" CELLSPACING=\"0\">";

			// Input row (down flow only)
			if (!inputs.empty()) {
				for (unsigned i = n_inputs; i < n_cols + 1; ++i) {
					inputs += "<TD BORDER=\"0\"></TD>";
				}
				html += std::string("<TR>") + inputs + "</TR>";
			}

			// Label row
			std::stringstream colspan;
			colspan << (flow_right ? 1 : (n_cols + 1));
			html += std::string("<TR><TD BORDER=\"0\" CELLPADDING=\"2\" COLSPAN=\"")
				+ colspan.str()
				+ "\">";
			const char* label = ganv_node_get_label(GANV_NODE(m));
			if (label) {
				html += label;
			}
			html += "</TD></TR>";

			// Ports rows (right flow only)
			if (!ports.empty()) {
				html += ports;
			}

			// Output row (down flow only)
			if (!outputs.empty()) {
				for (unsigned i = n_outputs; i < n_cols + 1; ++i) {
					outputs += "<TD BORDER=\"0\"></TD>";
				}
				html += std::string("<TR>") + outputs + "</TR>";
			}
			html += "</TABLE>";
			
			char* html_label_str = agstrdup_html(G, (char*)html.c_str());

			agsafeset(node, (char*)"label", (char*)html_label_str, (char*)"");
		} else if (GANV_IS_CIRCLE(*i)) {
			agsafeset(node, (char*)"shape", (char*)"circle", (char*)"");
			agsafeset(node, (char*)"fixedsize", (char*)"true", (char*)"");
			agsafeset(node, (char*)"margin", (char*)"0.0,0.0", (char*)"");

			const double radius   = ganv_circle_get_radius(GANV_CIRCLE(*i));
			const double penwidth = ganv_node_get_border_width(GANV_NODE(*i));
			const double span     = (radius + penwidth) * 2.3 / dpi;
			gv_set(node, (char*)"width", span);
			gv_set(node, (char*)"height", span);
			gv_set(node, (char*)"penwidth", penwidth);

			if (ganv_node_get_dash_length(GANV_NODE(*i)) > 0.0) {
				agsafeset(node, (char*)"style", (char*)"dashed", (char*)"");
			}
				
			const char* label = ganv_node_get_label(GANV_NODE(*i));
			if (label) {
				agsafeset(node, (char*)"label", (char*)label, (char*)"");
			} else {
				agsafeset(node, (char*)"label", (char*)"", (char*)"");
			}
		} else {
			std::cerr << "Unable to arrange item of unknown type" << std::endl;
		}
	}

	FOREACH_EDGE(_edges, i) {
		const GanvEdge* const edge   = *i;
		GVNodes::iterator     tail_i = nodes.find(edge->impl->tail);
		GVNodes::iterator     head_i = nodes.find(edge->impl->head);

		if (tail_i != nodes.end() && head_i != nodes.end()) {
			Agedge_t* e = agedge(G, tail_i->second, head_i->second, NULL, true);
			if (GANV_IS_PORT(edge->impl->tail)) {
				ss.str((char*)"");
				ss << edge->impl->tail << (flow_right ? ":e" : ":s");
				agsafeset(e, (char*)"tailport", (char*)ss.str().c_str(), (char*)"");
			}
			if (GANV_IS_PORT(edge->impl->head)) {
				ss.str((char*)"");
				ss << edge->impl->head << (flow_right ? ":w" : ":n");
				agsafeset(e, (char*)"headport", (char*)ss.str().c_str(), (char*)"");
			}
		} else {
			std::cerr << "Unable to find graphviz node" << std::endl;
		}
	}

	// Add edges between partners to have them lined up as if connected
	for (GVNodes::iterator i = nodes.begin(); i != nodes.end(); ++i) {
		GanvNode* partner = ganv_node_get_partner(i->first);
		if (partner) {
			GVNodes::iterator p = nodes.find(partner);
			if (p != nodes.end()) {
				Agedge_t* e = agedge(G, i->second, p->second, NULL, true);
				agsafeset(e, (char*)"style", (char*)"dotted", (char*)"");
			}
		}
	}

	gvLayout(gvc, G, (char*)"dot");
	FILE* tmp = fopen("/dev/null", "w");
	gvRender(gvc, G, (char*)"dot", tmp);
	fclose(tmp);

	if (filename != "") {
		FILE* fd = fopen(filename.c_str(), "w");
		gvRender(gvc, G, (char*)"dot", fd);
		fclose(fd);
	}

	return nodes;
}
#endif

#ifdef GANV_FDGL

inline Region
get_region(GanvNode* node)
{
	GanvItem* item = &node->item;

	double x1, y1, x2, y2;
	ganv_item_get_bounds(item, &x1, &y1, &x2, &y2);

	Region reg;
	ganv_item_get_bounds(item, &reg.pos.x, &reg.pos.y, &reg.area.x, &reg.area.y);
	reg.area.x = x2 - x1;
	reg.area.y = y2 - y1;
	reg.pos.x  = item->x + (reg.area.x / 2.0);
	reg.pos.y  = item->y + (reg.area.y / 2.0);

	// No need for i2w here since we only care about top-level items
	return reg;
}

inline void
apply_force(GanvNode* a, GanvNode* b, const Vector& f)
{
	a->impl->force = vec_add(a->impl->force, f);
	b->impl->force = vec_sub(b->impl->force, f);
}

gboolean
GanvCanvasImpl::layout_iteration()
{
	static const double T_PER_US = .0001;  // Sym time per real microsecond

	static uint64_t prev = 0;  // Previous iteration time

	const uint64_t now         = g_get_monotonic_time();
	const double   time_to_run = std::min((now - prev) * T_PER_US, 10.0);

	prev = now;

	const double QUANTUM  = 0.05;
	double       sym_time = 0.0;
	while (sym_time + QUANTUM < time_to_run) {
		if (!layout_calculate(QUANTUM, FALSE)) {
			break;
		}
		sym_time += QUANTUM;
	}

	return layout_calculate(QUANTUM, TRUE);
}

gboolean
GanvCanvasImpl::layout_calculate(double dur, bool update)
{
	// A light directional force to push sources to the top left
	static const double DIR_MAGNITUDE = -2000.0;
	Vector              dir           = { 0.0, 0.0 };
	switch (_gcanvas->direction) {
	case GANV_DIRECTION_RIGHT: dir.x = DIR_MAGNITUDE; break;
	case GANV_DIRECTION_DOWN:  dir.y = DIR_MAGNITUDE; break;
	}

	// Calculate attractive spring forces for edges
	FOREACH_EDGE(_edges, i) {
		const GanvEdge* const edge = *i;
		GanvNode*             tail = ganv_edge_get_tail(edge);
		GanvNode*             head = ganv_edge_get_head(edge);
		if (GANV_IS_PORT(tail)) {
			tail = GANV_NODE(ganv_port_get_module(GANV_PORT(tail)));
		}
		if (GANV_IS_PORT(head)) {
			head = GANV_NODE(ganv_port_get_module(GANV_PORT(head)));
		}
		if (tail == head) {
			continue;
		}

		head->impl->connected = tail->impl->connected = TRUE;

		GanvEdgeCoords coords;
		ganv_edge_get_coords(edge, &coords);

		const Vector tpos = { coords.x1, coords.y1 };
		const Vector hpos = { coords.x2, coords.y2 };
		apply_force(tail, head, edge_force(dir, hpos, tpos));
	}

	// Calculate repelling forces between nodes
	FOREACH_ITEM(_items, i) {
		if (!GANV_IS_MODULE(*i) && !GANV_IS_CIRCLE(*i)) {
			continue;
		}

		GanvNode* const node    = *i;
		GanvNode*       partner = ganv_node_get_partner(node);
		if (!partner && !node->impl->connected) {
			continue;
		}

		const Region reg = get_region(node);
		if (partner) {
			// Add fake long spring to partner to line up as if connected
			const Region preg = get_region(partner);
			apply_force(node, partner, edge_force(dir, preg.pos, reg.pos));
		}


		/* Add tide force which pulls all objects as if the layout is happening
		   on a flowing river surface.  This prevents disconnected components
		   from being ejected, since at some point the tide force will be
		   greater than distant repelling charges. */
		const Vector mouth = { -100000.0, -100000.0 };
		node->impl->force = vec_add(
			node->impl->force,
			tide_force(mouth, reg.pos, 4000000000000.0));

		FOREACH_ITEM(_items, j) {
			if (i == j || (!GANV_IS_MODULE(*i) && !GANV_IS_CIRCLE(*i))) {
				continue;
			}
			apply_force(node, *j, repel_force(reg, get_region(*j)));
		}
	}

	// Update positions based on calculated forces
	size_t n_moved = 0;
	FOREACH_ITEM(_items, i) {
		if (!GANV_IS_MODULE(*i) && !GANV_IS_CIRCLE(*i)) {
			continue;
		}

		GanvNode* const node = *i;

		static const float damp = 0.2;  // Velocity damping

		if (node->impl->grabbed || !node->impl->connected) {
			node->impl->vel.x = 0.0;
			node->impl->vel.y = 0.0;
		} else {
			node->impl->vel = vec_add(node->impl->vel,
			                          vec_mult(node->impl->force, dur));
			node->impl->vel = vec_mult(node->impl->vel, damp);

			static const double MAX_VEL   = 1000.0;
			static const double MIN_COORD = 4.0;

			// Clamp velocity
			const double vel_mag = vec_mag(node->impl->vel);
			if (vel_mag > MAX_VEL) {
				node->impl->vel = vec_mult(
					vec_mult(node->impl->vel, 1.0 / vel_mag),
					MAX_VEL);
			}
				                           
			// Update position
			GanvItem*    item = &node->item;
			const double x0   = item->x;
			const double y0   = item->y;
			const Vector dpos = vec_mult(node->impl->vel, dur);

			item->x = std::max(MIN_COORD, item->x + dpos.x);
			item->y = std::max(MIN_COORD, item->y + dpos.y);

			if (update) {
				ganv_item_request_update(item);
				item->canvas->need_repick = TRUE;
			}

			if (lrint(x0) != lrint(item->x) || lrint(y0) != lrint(item->y)) {
				++n_moved;
			}
		}

		// Reset forces for next time
		node->impl->force.x   = 0.0;
		node->impl->force.y   = 0.0;
		node->impl->connected = FALSE;
	}

	if (update) {
		// Now update edge positions to reflect new node positions
		FOREACH_EDGE(_edges, i) {
			GanvEdge* const edge = *i;
			ganv_edge_update_location(edge);
		}
	}

	return n_moved > 0;
}

#endif // GANV_FDGL

void
GanvCanvasImpl::remove_edge(GanvEdge* edge)
{
	if (edge) {
		_selected_edges.erase(edge);
		_edges.erase(edge);
		_dst_edges.erase(edge);
		ganv_edge_request_redraw(GANV_ITEM(edge)->canvas, &edge->impl->coords);
		gtk_object_destroy(GTK_OBJECT(edge));
		contents_changed();
	}
}

GanvEdge*
GanvCanvasImpl::get_edge_between(const GanvNode* tail,
                                 const GanvNode* head) const
{
	GanvEdgeKey key;
	make_edge_search_key(&key, tail, head);
	Edges::const_iterator i = _edges.find((GanvEdge*)&key);
	return (i != _edges.end()) ? *i : NULL;
}

/** Return whether there is a edge between item1 and item2.
 *
 * Note that edges are directed, so this may return false when there
 * is a edge between the two items (in the opposite direction).
 */
bool
GanvCanvasImpl::are_connected(const GanvNode* tail,
                              const GanvNode* head) const
{
	return get_edge_between(tail, head) != NULL;
}

void
GanvCanvasImpl::select_port(GanvPort* p, bool unique)
{
	if (unique) {
		unselect_ports();
	}
	g_object_set(G_OBJECT(p), "selected", TRUE, NULL);
	_selected_ports.insert(p);
	_last_selected_port = p;
}

void
GanvCanvasImpl::select_port_toggle(GanvPort* port, int mod_state)
{
	gboolean selected;
	g_object_get(G_OBJECT(port), "selected", &selected, NULL);
	if ((mod_state & GDK_CONTROL_MASK)) {
		if (selected)
			unselect_port(port);
		else
			select_port(port);
	} else if ((mod_state & GDK_SHIFT_MASK)) {
		GanvModule* const m = ganv_port_get_module(port);
		if (_last_selected_port && m
		    && ganv_port_get_module(_last_selected_port) == m) {
			// Pivot around _last_selected_port in a single pass over module ports each click
			GanvPort* old_last_selected = _last_selected_port;
			GanvPort* first             = NULL;
			bool      done              = false;
			for (size_t i = 0; i < ganv_module_num_ports(m); ++i) {
				GanvPort* const p = ganv_module_get_port(m, i);
				if (!first && !done && (p == _last_selected_port || p == port)) {
					first = p;
				}

				if (first && !done && p->impl->is_input == first->impl->is_input) {
					select_port(p, false);
				} else {
					unselect_port(p);
				}

				if (p != first && (p == old_last_selected || p == port)) {
					done = true;
				}
			}
			_last_selected_port = old_last_selected;
		} else {
			if (selected) {
				unselect_port(port);
			} else {
				select_port(port);
			}
		}
	} else {
		if (selected) {
			unselect_ports();
		} else {
			select_port(port, true);
		}
	}
}

void
GanvCanvasImpl::unselect_port(GanvPort* p)
{
	_selected_ports.erase(p);
	g_object_set(G_OBJECT(p), "selected", FALSE, NULL);
	if (_last_selected_port == p) {
		_last_selected_port = NULL;
	}
}

void
GanvCanvasImpl::selection_joined_with(GanvPort* port)
{
	FOREACH_SELECTED_PORT(i)
		ports_joined(*i, port);
}

void
GanvCanvasImpl::join_selection()
{
	vector<GanvPort*> inputs;
	vector<GanvPort*> outputs;
	FOREACH_SELECTED_PORT(i) {
		if ((*i)->impl->is_input) {
			inputs.push_back(*i);
		} else {
			outputs.push_back(*i);
		}
	}

	if (inputs.size() == 1) { // 1 -> n
		for (size_t i = 0; i < outputs.size(); ++i)
			ports_joined(inputs[0], outputs[i]);
	} else if (outputs.size() == 1) { // n -> 1
		for (size_t i = 0; i < inputs.size(); ++i)
			ports_joined(inputs[i], outputs[0]);
	} else { // n -> m
		size_t num_to_connect = std::min(inputs.size(), outputs.size());
		for (size_t i = 0; i < num_to_connect; ++i) {
			ports_joined(inputs[i], outputs[i]);
		}
	}
}

GanvNode*
GanvCanvasImpl::get_node_at(double x, double y)
{
	GanvItem* item = ganv_canvas_base_get_item_at(GANV_CANVAS_BASE(_gcanvas), x, y);
	while (item) {
		if (GANV_IS_NODE(item)) {
			return GANV_NODE(item);
		} else {
			item = item->parent;
		}
	}

	return NULL;
}

bool
GanvCanvasImpl::on_event(GdkEvent* event)
{
	static const int scroll_increment = 10;
	int scroll_x, scroll_y;

	bool handled = false;
	switch (event->type) {
	case GDK_KEY_PRESS:
		handled = true;
		ganv_canvas_base_get_scroll_offsets(GANV_CANVAS_BASE(_gcanvas), &scroll_x, &scroll_y);
		switch (event->key.keyval) {
		case GDK_Up:
			scroll_y -= scroll_increment;
			break;
		case GDK_Down:
			scroll_y += scroll_increment;
			break;
		case GDK_Left:
			scroll_x -= scroll_increment;
			break;
		case GDK_Right:
			scroll_x += scroll_increment;
			break;
		case GDK_Return:
			if (_selected_ports.size() > 1) {
				join_selection();
				clear_selection();
			}
			break;
		default:
			handled = false;
		}
		if (handled) {
			ganv_canvas_base_scroll_to(GANV_CANVAS_BASE(_gcanvas), scroll_x, scroll_y);
			return true;
		}
		break;

	case GDK_SCROLL:
		if ((event->scroll.state & GDK_CONTROL_MASK)) {
			const double font_size = ganv_canvas_get_font_size(_gcanvas);
			if (event->scroll.direction == GDK_SCROLL_UP) {
				ganv_canvas_set_font_size(_gcanvas, font_size * 1.25);
				return true;
			} else if (event->scroll.direction == GDK_SCROLL_DOWN) {
				ganv_canvas_set_font_size(_gcanvas, font_size * 0.75);
				return true;
			}
		}
		break;

	default:
		break;
	}

	return scroll_drag_handler(event)
		|| select_drag_handler(event)
		|| connect_drag_handler(event);
}

bool
GanvCanvasImpl::scroll_drag_handler(GdkEvent* event)
{
	bool handled = true;

	static int    original_scroll_x = 0;
	static int    original_scroll_y = 0;
	static double origin_x          = 0;
	static double origin_y          = 0;
	static double scroll_offset_x   = 0;
	static double scroll_offset_y   = 0;
	static double last_x            = 0;
	static double last_y            = 0;

	if (event->type == GDK_BUTTON_PRESS && event->button.button == 2) {
		ganv_item_grab(
			GANV_ITEM(root()),
			GDK_POINTER_MOTION_MASK|GDK_BUTTON_RELEASE_MASK,
			NULL, event->button.time);
		ganv_canvas_base_get_scroll_offsets(GANV_CANVAS_BASE(_gcanvas), &original_scroll_x, &original_scroll_y);
		scroll_offset_x = 0;
		scroll_offset_y = 0;
		origin_x = event->button.x_root;
		origin_y = event->button.y_root;
		//cerr << "Origin: (" << origin_x << "," << origin_y << ")\n";
		last_x = origin_x;
		last_y = origin_y;
		_drag_state = SCROLL;

	} else if (event->type == GDK_MOTION_NOTIFY && _drag_state == SCROLL) {
		const double x        = event->motion.x_root;
		const double y        = event->motion.y_root;
		const double x_offset = last_x - x;
		const double y_offset = last_y - y;

		//cerr << "Coord: (" << x << "," << y << ")\n";
		//cerr << "Offset: (" << x_offset << "," << y_offset << ")\n";

		scroll_offset_x += x_offset;
		scroll_offset_y += y_offset;
		ganv_canvas_base_scroll_to(GANV_CANVAS_BASE(_gcanvas),
		                       lrint(original_scroll_x + scroll_offset_x),
		                       lrint(original_scroll_y + scroll_offset_y));
		last_x = x;
		last_y = y;
	} else if (event->type == GDK_BUTTON_RELEASE && _drag_state == SCROLL) {
		ganv_item_ungrab(GANV_ITEM(root()), event->button.time);
		_drag_state = NOT_DRAGGING;
	} else {
		handled = false;
	}

	return handled;
}

static void
get_motion_coords(GdkEventMotion* motion, double* x, double* y)
{
	if (motion->is_hint) {
		gint px;
		gint py;
		GdkModifierType state;
		gdk_window_get_pointer(motion->window, &px, &py, &state);
		*x = px;
		*y = py;
	} else {
		*x = motion->x;
		*y = motion->y;
	}
}

bool
GanvCanvasImpl::select_drag_handler(GdkEvent* event)
{
	if (event->type == GDK_BUTTON_PRESS && event->button.button == 1) {
		assert(_select_rect == NULL);
		_drag_state = SELECT;
		if ( !(event->button.state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) )
			clear_selection();
		_select_rect = GANV_BOX(
			ganv_item_new(
				GANV_ITEM(root()),
				ganv_box_get_type(),
				"x1", event->button.x,
				"y1", event->button.y,
				"x2", event->button.x,
				"y2", event->button.y,
				"fill-color", SELECT_RECT_FILL_COLOUR,
				"border-color", SELECT_RECT_BORDER_COLOUR,
				NULL));
		_select_start_x = event->button.x;
		_select_start_y = event->button.y;
		ganv_item_grab(
			GANV_ITEM(root()), GDK_POINTER_MOTION_MASK|GDK_BUTTON_RELEASE_MASK,
			NULL, event->button.time);
		return true;
	} else if (event->type == GDK_MOTION_NOTIFY && _drag_state == SELECT) {
		assert(_select_rect);
		double x, y;
		get_motion_coords(&event->motion, &x, &y);
		_select_rect->impl->coords.x1 = MIN(_select_start_x, x);
		_select_rect->impl->coords.y1 = MIN(_select_start_y, y);
		_select_rect->impl->coords.x2 = MAX(_select_start_x, x);
		_select_rect->impl->coords.y2 = MAX(_select_start_y, y);
		ganv_item_request_update(&_select_rect->node.item);
		return true;
	} else if (event->type == GDK_BUTTON_RELEASE && _drag_state == SELECT) {
		// Normalize select rect
		ganv_box_normalize(_select_rect);

		// Select all modules within rect
		FOREACH_ITEM(_items, i) {
			GanvNode* node = *i;
			if ((void*)node != (void*)_select_rect &&
			    ganv_node_is_within(
				    node,
				    ganv_box_get_x1(_select_rect),
				    ganv_box_get_y1(_select_rect),
				    ganv_box_get_x2(_select_rect),
				    ganv_box_get_y2(_select_rect))) {
				gboolean selected;
				g_object_get(G_OBJECT(node), "selected", &selected, NULL);
				if (selected) {
					unselect_item(node);
				} else {
					select_item(node);
				}
			}
		}

		// Select all edges with handles within rect
		FOREACH_EDGE(_edges, i) {
			if (ganv_edge_is_within(
				    (*i),
				    ganv_box_get_x1(_select_rect),
				    ganv_box_get_y1(_select_rect),
				    ganv_box_get_x2(_select_rect),
				    ganv_box_get_y2(_select_rect))) {
				select_edge(*i);
			}
		}

		ganv_item_ungrab(GANV_ITEM(root()), event->button.time);

		gtk_object_destroy(GTK_OBJECT(_select_rect));
		_select_rect = NULL;
		_drag_state = NOT_DRAGGING;
		return true;
	}
	return false;
}

bool
GanvCanvasImpl::connect_drag_handler(GdkEvent* event)
{
	static bool snapped = false;

	if (_drag_state != EDGE) {
		return false;
	}

	if (event->type == GDK_MOTION_NOTIFY) {
		double x, y;
		get_motion_coords(&event->motion, &x, &y);

		if (!_drag_edge) {
			// Create drag edge
			assert(!_drag_node);
			assert(_connect_port);

			_drag_node = GANV_NODE(
				ganv_item_new(
					GANV_ITEM(ganv_canvas_base_root(GANV_CANVAS_BASE(_gcanvas))),
					ganv_node_get_type(),
					"x", x,
					"y", y,
					NULL));

			_drag_edge = ganv_edge_new(
				_gcanvas,
				GANV_NODE(_connect_port),
				_drag_node,
				"color", GANV_NODE(_connect_port)->impl->fill_color,
				"curved", TRUE,
				"ghost", TRUE,
				NULL);
		}

		GanvNode* joinee = get_node_at(x, y);
		if (joinee && ganv_node_can_head(joinee) && joinee != _drag_node) {
			// Snap to item
			snapped = true;
			ganv_item_set(&_drag_edge->item, "head", joinee, NULL);
		} else if (snapped) {
			// Unsnap from item
			snapped = false;
			ganv_item_set(&_drag_edge->item, "head", _drag_node, NULL);
		}

		// Update drag edge for pointer position
		ganv_node_move_to(_drag_node, x, y);
		ganv_item_request_update(GANV_ITEM(_drag_node));
		ganv_item_request_update(GANV_ITEM(_drag_edge));

		return true;

	} else if (event->type == GDK_BUTTON_RELEASE) {
		ganv_item_ungrab(GANV_ITEM(root()), event->button.time);

		double x = event->button.x;
		double y = event->button.y;

		GanvNode* joinee = get_node_at(x, y);

		if (GANV_IS_PORT(joinee)) {
			if (joinee == GANV_NODE(_connect_port)) {
				// Drag ended on the same port it started on, port clicked
				if (_selected_ports.empty()) {
					// No selected ports, port clicked
					select_port(_connect_port);
				} else {
					// Connect to selected ports
					selection_joined_with(_connect_port);
					unselect_ports();
					_connect_port = NULL;
				}
			} else {  // drag ended on different port
				ports_joined(_connect_port, GANV_PORT(joinee));
				unselect_ports();
				_connect_port = NULL;
			}
		}

		unselect_ports();
		end_connect_drag();
		return true;
	}

	return false;
}

void
GanvCanvasImpl::end_connect_drag()
{
	if (_connect_port) {
		g_object_set(G_OBJECT(_connect_port), "highlighted", FALSE, NULL);
	}
	gtk_object_destroy(GTK_OBJECT(_drag_edge));
	gtk_object_destroy(GTK_OBJECT(_drag_node));
	_drag_state   = NOT_DRAGGING;
	_connect_port = NULL;
	_drag_edge    = NULL;
	_drag_node    = NULL;
}

bool
GanvCanvasImpl::port_event(GdkEvent* event, GanvPort* port)
{
	static bool   port_pressed        = true;
	static bool   port_dragging       = false;
	static bool   control_dragging    = false;
	static double control_start_x     = 0;
	static double control_start_y     = 0;
	static float  control_start_value = 0;

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		if (event->button.button == 1) {
			GanvModule* const module = ganv_port_get_module(port);
			double port_x = event->button.x;
			double port_y = event->button.y;
			ganv_item_w2i(GANV_ITEM(port), &port_x, &port_y);

			if (module && port->impl->control &&
			    (port->impl->is_input ||
			     (port->impl->is_controllable &&
			      port_x < ganv_box_get_width(GANV_BOX(port)) / 2.0))) {
				if (port->impl->control->is_toggle) {
					if (port->impl->control->value >= 0.5) {
						ganv_port_set_control_value_internal(port, 0.0);
					} else {
						ganv_port_set_control_value_internal(port, 1.0);
					}
				} else {
					control_dragging    = port_pressed = true;
					control_start_x     = event->button.x_root;
					control_start_y     = event->button.y_root;
					control_start_value = ganv_port_get_control_value(port);
					ganv_item_grab(GANV_ITEM(port),
					               GDK_POINTER_MOTION_MASK|GDK_BUTTON_RELEASE_MASK,
					               NULL, event->button.time);
					GANV_NODE(port)->impl->grabbed = TRUE;

				}
			} else if (!port->impl->is_input) {
				port_dragging = port_pressed = true;
				ganv_item_grab(GANV_ITEM(port),
				               GDK_BUTTON_RELEASE_MASK|GDK_POINTER_MOTION_MASK|
				               GDK_ENTER_NOTIFY_MASK|GDK_LEAVE_NOTIFY_MASK,
				               NULL, event->button.time);
			} else {
				port_pressed = true;
				ganv_item_grab(GANV_ITEM(port),
				               GDK_BUTTON_RELEASE_MASK,
				               NULL, event->button.time);
			}
			return true;
		}
		break;

	case GDK_MOTION_NOTIFY:
		if (control_dragging) {
			const double mouse_x       = event->button.x_root;
			const double mouse_y       = event->button.y_root;
			GdkScreen*   screen        = gdk_screen_get_default();
			const int    screen_width  = gdk_screen_get_width(screen);
			const int    screen_height = gdk_screen_get_height(screen);
			const double drag_dx       = mouse_x - control_start_x;
			const double drag_dy       = mouse_y - control_start_y;
			const double xpad          = 8.0;  // Pad from screen edge
			const double ythresh       = 0.2;  // Minimum y fraction for fine

			const double range_x = ((drag_dx > 0)
			                        ? (screen_width - control_start_x)
			                        : control_start_x) - xpad;

			const double range_y = ((drag_dy > 0)
			                        ? (screen_height - control_start_y)
			                        : control_start_y);

			const double dx = drag_dx / range_x;
			const double dy = fabs(drag_dy / range_y);

			const double value_range = (drag_dx > 0)
				? port->impl->control->max - control_start_value
				: control_start_value - port->impl->control->min;

			const double sens = (dy < ythresh)
				? 1.0
				: 1.0 - fabs(drag_dy / (range_y + ythresh));

			const double dvalue = (dx * value_range) * sens;
			double       value  = control_start_value + dvalue;
			if (value < port->impl->control->min) {
				value = port->impl->control->min;
			} else if (value > port->impl->control->max) {
				value = port->impl->control->max;
			}
			ganv_port_set_control_value_internal(port, value);
			return true;
		}
		break;

	case GDK_BUTTON_RELEASE:
		if (port_pressed) {
			ganv_item_ungrab(GANV_ITEM(port), event->button.time);
		}

		if (port_dragging) {
			if (_connect_port) { // dragging
				ports_joined(port, _connect_port);
				unselect_ports();
			} else {
				bool modded = event->button.state & (GDK_SHIFT_MASK|GDK_CONTROL_MASK);
				if (!modded && _last_selected_port && _last_selected_port->impl->is_input != port->impl->is_input) {
					selection_joined_with(port);
					unselect_ports();
				} else {
					select_port_toggle(port, event->button.state);
				}
			}
			port_dragging = false;
		} else if (control_dragging) {
			control_dragging = false;
			GANV_NODE(port)->impl->grabbed = FALSE;
			if (event->button.x_root == control_start_x &&
			    event->button.y_root == control_start_y) {
				select_port_toggle(port, event->button.state);
			}
		} else if (event->button.state & (GDK_SHIFT_MASK|GDK_CONTROL_MASK)) {
			select_port_toggle(port, event->button.state);
		} else {
			selection_joined_with(port);
		}
		return true;

	case GDK_ENTER_NOTIFY:
		gboolean selected;
		g_object_get(G_OBJECT(port), "selected", &selected, NULL);
		if (!control_dragging && !selected) {
			g_object_set(G_OBJECT(port), "highlighted", TRUE, NULL);
			return true;
		}
		break;

	case GDK_LEAVE_NOTIFY:
		if (port_dragging) {
			_drag_state = GanvCanvasImpl::EDGE;
			_connect_port = port;
			port_dragging = false;
			ganv_item_ungrab(GANV_ITEM(port), event->crossing.time);
			ganv_item_grab(
				GANV_ITEM(root()),
				GDK_BUTTON_PRESS_MASK|GDK_POINTER_MOTION_MASK|GDK_BUTTON_RELEASE_MASK,
				NULL, event->crossing.time);
			return true;
		} else if (!control_dragging) {
			g_object_set(G_OBJECT(port), "highlighted", FALSE, NULL);
			return true;
		}
		break;

	default:
		break;
	}

	return false;
}

/** Called when two ports are 'joined' (connected or disconnected)
 */
void
GanvCanvasImpl::ports_joined(GanvPort* port1, GanvPort* port2)
{
	if (port1 == port2 || !port1 || !port2 || !port1->impl || !port2->impl) {
		return;
	}

	g_object_set(G_OBJECT(port1), "highlighted", FALSE, NULL);
	g_object_set(G_OBJECT(port2), "highlighted", FALSE, NULL);

	GanvNode* src_node;
	GanvNode* dst_node;

	if (port2->impl->is_input && !port1->impl->is_input) {
		src_node = GANV_NODE(port1);
		dst_node = GANV_NODE(port2);
	} else if (!port2->impl->is_input && port1->impl->is_input) {
		src_node = GANV_NODE(port2);
		dst_node = GANV_NODE(port1);
	} else {
		return;
	}

	if (!are_connected(src_node, dst_node)) {
		g_signal_emit(_gcanvas, signal_connect, 0,
		              src_node, dst_node, NULL);
	} else {
		g_signal_emit(_gcanvas, signal_disconnect, 0,
		              src_node, dst_node, NULL);
	}
}

/** Update animated "rubber band" selection effect. */
bool
GanvCanvasImpl::animate_selected()
{
#ifdef g_get_monotonic_time
	// Only available in glib 2.28
	const double seconds = g_get_monotonic_time() / 1000000.0;
#else
	GTimeVal time;
	g_get_current_time(&time);
	const double seconds = time.tv_sec + time.tv_usec / (double)G_USEC_PER_SEC;
#endif

	FOREACH_ITEM(_selected_items, s) {
		ganv_node_tick(*s, seconds);
	}

	FOREACH_SELECTED_PORT(p) {
		ganv_node_tick(GANV_NODE(*p), seconds);
	}

	FOREACH_EDGE(_selected_edges, c) {
		ganv_edge_tick(*c, seconds);
	}

	return true;
}

void
GanvCanvasImpl::move_contents_to_internal(double x, double y, double min_x, double min_y)
{
	FOREACH_ITEM(_items, i) {
		ganv_node_move(*i,
		               x - min_x,
		               y - min_y);
	}
}

void
GanvCanvasImpl::for_each_node(GanvNodeFunc f,
                              void*        data)
{
	FOREACH_ITEM(_items, i) {
		f(*i, data);
	}
}

void
GanvCanvasImpl::for_each_edge_from(const GanvNode* tail,
                                   GanvEdgeFunc    f,
                                   void*           data)
{
	for (GanvCanvasImpl::Edges::const_iterator i = first_edge_from(tail);
	     i != _edges.end() && (*i)->impl->tail == tail;) {
		GanvCanvasImpl::Edges::const_iterator next = i;
		++next;
		f((*i), data);
		i = next;
	}
}

void
GanvCanvasImpl::for_each_edge_to(const GanvNode* head,
                                 GanvEdgeFunc    f,
                                 void*           data)
{
	for (GanvCanvasImpl::Edges::const_iterator i = first_edge_to(head);
	     i != _dst_edges.end() && (*i)->impl->head == head;) {
		GanvCanvasImpl::Edges::const_iterator next = i;
		++next;
		f((*i), data);
		i = next;
	}
}

void
GanvCanvasImpl::for_each_edge_on(const GanvNode* node,
                                 GanvEdgeFunc    f,
                                 void*           data)
{
	for_each_edge_from(node, f, data);
	for_each_edge_to(node, f, data);
}

void
GanvCanvasImpl::unselect_ports()
{
	for (GanvCanvasImpl::SelectedPorts::iterator i = _selected_ports.begin();
	     i != _selected_ports.end(); ++i)
		g_object_set(G_OBJECT(*i), "selected", FALSE, NULL);

	_selected_ports.clear();
	_last_selected_port = NULL;
}

void
GanvCanvasImpl::clear_selection()
{
	unselect_ports();

	Items items(_selected_items);
	_selected_items.clear();
	FOREACH_ITEM(items, i) {
		ganv_item_set(GANV_ITEM(*i), "selected", FALSE, NULL);
	}

	SelectedEdges edges(_selected_edges);
	FOREACH_SELECTED_EDGE(edges, c) {
		ganv_item_set(GANV_ITEM(*c), "selected", FALSE, NULL);
	}
}

void
GanvCanvasImpl::resize(double width, double height)
{
	if (width != _gcanvas->width || height != _gcanvas->height) {
		_gcanvas->width  = width;
		_gcanvas->height = height;
		ganv_canvas_base_set_scroll_region(GANV_CANVAS_BASE(_gcanvas),
		                               0.0, 0.0, width, height);
	}
}

void
GanvCanvasImpl::contents_changed()
{
#ifdef GANV_FDGL
	if (!_layout_idle_id) {
		_layout_idle_id = g_timeout_add_full(
			G_PRIORITY_DEFAULT_IDLE,
			33,
			on_layout_timeout,
			this,
			on_layout_done);
	}
#endif
}

void
GanvCanvasImpl::set_zoom_and_font_size(double zoom, double points)
{
	points = std::max(points, 1.0);
	zoom   = std::max(zoom, 0.01);

	if (zoom == _zoom && points == _font_size)
		return;

	if (zoom != _zoom) {
		_zoom = zoom;
		ganv_canvas_base_set_pixels_per_unit(GANV_CANVAS_BASE(_gcanvas), zoom);
	}

	_font_size = points;

	FOREACH_ITEM(_items, i) {
		ganv_node_redraw_text(*i);
	}
}

namespace Ganv {

static gboolean
on_event_after(GanvItem* canvasitem,
               GdkEvent* ev,
               void*     canvas)
{
	return ((Canvas*)canvas)->signal_event.emit(ev);
}

static void
on_connect(GanvCanvas* canvas, GanvNode* tail, GanvNode* head, void* data)
{
	Canvas* canvasmm = (Canvas*)data;
	canvasmm->signal_connect.emit(Glib::wrap(tail), Glib::wrap(head));
}

static void
on_disconnect(GanvCanvas* canvas, GanvNode* tail, GanvNode* head, void* data)
{
	Canvas* canvasmm = (Canvas*)data;
	canvasmm->signal_disconnect.emit(Glib::wrap(tail), Glib::wrap(head));
}

Canvas::Canvas(double width, double height)
	: _gobj(GANV_CANVAS(ganv_canvas_new(width, height)))
{
	g_object_set_qdata(G_OBJECT(impl()->_gcanvas), wrapper_key(), this);

	g_signal_connect_after(impl()->root(), "event",
	                       G_CALLBACK(on_event_after), this);
	g_signal_connect(gobj(), "connect",
	                 G_CALLBACK(on_connect), this);
	g_signal_connect(gobj(), "disconnect",
	                 G_CALLBACK(on_disconnect), this);

	_animate_connection = Glib::signal_timeout().connect(
		sigc::mem_fun(impl(), &GanvCanvasImpl::animate_selected), 120);
}

Canvas::~Canvas()
{
	_animate_connection.disconnect();
	clear();
	delete impl();
}

void
Canvas::remove_edge(Node* item1, Node* item2)
{
	Edge* edge = get_edge(item1, item2);
	if (edge) {
		impl()->remove_edge(edge->gobj());
	}
}

void
Canvas::remove_edge(Edge* edge)
{
	impl()->remove_edge(edge->gobj());
}

Edge*
Canvas::get_edge(Node* tail, Node* head) const
{
	GanvEdge* e = impl()->get_edge_between(tail->gobj(), head->gobj());
	if (e) {
		return Glib::wrap(e);
	} else {
		return NULL;
	}
}

void
Canvas::for_each_edge(GanvEdgeFunc f, void* data)
{
	for (GanvCanvasImpl::Edges::iterator i = impl()->_edges.begin();
	     i != impl()->_edges.end();) {
		GanvCanvasImpl::Edges::iterator next = i;
		++next;

		f((*i), data);

		i = next;
	}
}

void
Canvas::for_each_selected_edge(GanvEdgeFunc f, void* data)
{
	FOREACH_EDGE(impl()->_selected_edges, i) {
		f((*i), data);
	}
}

GanvItem*
Canvas::root()
{
	return ganv_canvas_base_root(GANV_CANVAS_BASE(impl()->_gcanvas));
}

Gtk::Layout&
Canvas::widget()
{
	return *Glib::wrap(impl()->_layout);
}

void
Canvas::get_scroll_offsets(int& cx, int& cy) const
{
	ganv_canvas_base_get_scroll_offsets(GANV_CANVAS_BASE(impl()->_gcanvas), &cx, &cy);
}

void
Canvas::scroll_to(int x, int y)
{
	ganv_canvas_base_scroll_to(GANV_CANVAS_BASE(impl()->_gcanvas), x, y);
}

GQuark
Canvas::wrapper_key()
{
	return impl()->_wrapper_key;
}

GanvCanvas*
Canvas::gobj()
{
	return impl()->_gcanvas;
}

const GanvCanvas*
Canvas::gobj() const
{
	return impl()->_gcanvas;
}

} // namespace Ganv

extern "C" {

#include "ganv/canvas-base.h"
#include "ganv/canvas.h"

#include "./boilerplate.h"
#include "./color.h"
#include "./gettext.h"

G_DEFINE_TYPE(GanvCanvas, ganv_canvas, GANV_TYPE_CANVAS_BASE)

static GanvCanvasBaseClass* parent_class;

enum {
	PROP_0,
	PROP_WIDTH,
	PROP_HEIGHT,
	PROP_DIRECTION,
	PROP_FONT_SIZE,
	PROP_LOCKED
};

static void
ganv_canvas_init(GanvCanvas* canvas)
{
	canvas->direction = GANV_DIRECTION_RIGHT;
	canvas->impl = new GanvCanvasImpl(canvas);
	canvas->impl->_font_size = ganv_canvas_get_default_font_size(canvas);
}

static void
ganv_canvas_set_property(GObject*      object,
                         guint         prop_id,
                         const GValue* value,
                         GParamSpec*   pspec)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(GANV_IS_CANVAS(object));

	GanvCanvas* canvas = GANV_CANVAS(object);

	switch (prop_id) {
	case PROP_WIDTH:
		ganv_canvas_resize(canvas, g_value_get_double(value), canvas->height);
		break;
	case PROP_HEIGHT:
		ganv_canvas_resize(canvas, canvas->width, g_value_get_double(value));
		break;
	case PROP_DIRECTION:
		ganv_canvas_set_direction(canvas, (GanvDirection)g_value_get_enum(value));
		break;
	case PROP_FONT_SIZE:
		ganv_canvas_set_font_size(canvas, g_value_get_double(value));
		break;
	case PROP_LOCKED:
		canvas->locked = g_value_get_boolean(value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
ganv_canvas_get_property(GObject*    object,
                         guint       prop_id,
                         GValue*     value,
                         GParamSpec* pspec)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(GANV_IS_CANVAS(object));

	GanvCanvas* canvas = GANV_CANVAS(object);

	switch (prop_id) {
		GET_CASE(WIDTH, double, canvas->width)
		GET_CASE(HEIGHT, double, canvas->height)
		GET_CASE(LOCKED, boolean, canvas->locked);
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
ganv_canvas_class_init(GanvCanvasClass* klass)
{
	GObjectClass* gobject_class = (GObjectClass*)klass;

	parent_class = GANV_CANVAS_BASE_CLASS(g_type_class_peek_parent(klass));

	gobject_class->set_property = ganv_canvas_set_property;
	gobject_class->get_property = ganv_canvas_get_property;

	g_object_class_install_property(
		gobject_class, PROP_WIDTH, g_param_spec_double(
			"width",
			_("Width"),
			_("The width of the canvas."),
			0.0, G_MAXDOUBLE,
			800.0,
			(GParamFlags)G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_HEIGHT, g_param_spec_double(
			"height",
			_("Height"),
			_("The height of the canvas"),
			0.0, G_MAXDOUBLE,
			600.0,
			(GParamFlags)G_PARAM_READWRITE));

	GEnumValue down_dir  = { GANV_DIRECTION_DOWN, "down", "down" };
	GEnumValue right_dir = { GANV_DIRECTION_RIGHT, "right", "right" };
	GEnumValue null_dir  = { 0, 0, 0 };
	dir_values[0] = down_dir;
	dir_values[1] = right_dir;
	dir_values[2] = null_dir;
	GType dir_type = g_enum_register_static("GanvDirection",
	                                        dir_values);

	g_object_class_install_property(
		gobject_class, PROP_DIRECTION, g_param_spec_enum(
			"direction",
			_("Direction"),
			_("The direction of the signal flow on the canvas."),
			dir_type,
			GANV_DIRECTION_RIGHT,
			(GParamFlags)G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_FONT_SIZE, g_param_spec_double(
			"font-size",
			_("Font size"),
			_("The default font size for the canvas"),
			0.0, G_MAXDOUBLE,
			12.0,
			(GParamFlags)G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_LOCKED, g_param_spec_boolean(
			"locked",
			_("Locked"),
			_("If true, nodes on the canvas can not be moved by the user."),
			FALSE,
			(GParamFlags)G_PARAM_READWRITE));

	signal_connect = g_signal_new("connect",
	                              ganv_canvas_get_type(),
	                              G_SIGNAL_RUN_FIRST,
	                              0, NULL, NULL,
	                              ganv_marshal_VOID__OBJECT_OBJECT,
	                              G_TYPE_NONE,
	                              2,
	                              ganv_node_get_type(),
	                              ganv_node_get_type(),
	                              0);

	signal_disconnect = g_signal_new("disconnect",
	                                 ganv_canvas_get_type(),
	                                 G_SIGNAL_RUN_FIRST,
	                                 0, NULL, NULL,
	                                 ganv_marshal_VOID__OBJECT_OBJECT,
	                                 G_TYPE_NONE,
	                                 2,
	                                 ganv_node_get_type(),
	                                 ganv_node_get_type(),
	                                 0);
}

GanvCanvas*
ganv_canvas_new(double width, double height)
{
	GanvCanvas* canvas = GANV_CANVAS(g_object_new(ganv_canvas_get_type(),
	                                              "width", width,
	                                              "height", height,
	                                              NULL));
	ganv_canvas_base_set_scroll_region(GANV_CANVAS_BASE(canvas),
	                               0.0, 0.0, width, height);
	return canvas;
}

void
ganv_canvas_resize(GanvCanvas* canvas, double width, double height)
{
	canvas->impl->resize(width, height);
}

void
ganv_canvas_contents_changed(GanvCanvas* canvas)
{
	canvas->impl->contents_changed();
}

GanvItem*
ganv_canvas_get_root(const GanvCanvas* canvas)
{
	return ganv_canvas_base_root(GANV_CANVAS_BASE(canvas->impl->_gcanvas));
}

double
ganv_canvas_get_default_font_size(const GanvCanvas* canvas)
{
	GtkStyle*                   style = gtk_rc_get_style(GTK_WIDGET(canvas));
	const PangoFontDescription* font  = style->font_desc;
	return pango_font_description_get_size(font) / (double)PANGO_SCALE;
}

double
ganv_canvas_get_font_size(const GanvCanvas* canvas)
{
	return canvas->impl->_font_size;
}

void
ganv_canvas_set_zoom(GanvCanvas* canvas, double zoom)
{
	canvas->impl->set_zoom_and_font_size(zoom, canvas->impl->_font_size);
}

void
ganv_canvas_set_font_size(GanvCanvas* canvas, double points)
{
	canvas->impl->set_zoom_and_font_size(canvas->impl->_zoom, points);
}

void
ganv_canvas_set_scale(GanvCanvas* canvas, double zoom, double points)
{
	canvas->impl->set_zoom_and_font_size(zoom, points);
}

void
ganv_canvas_zoom_full(GanvCanvas* canvas)
{
	if (canvas->impl->_items.empty())
		return;

	int win_width, win_height;
	GdkWindow* win = gtk_widget_get_window(
		GTK_WIDGET(GANV_CANVAS_BASE(canvas->impl->_gcanvas)));
	gdk_window_get_size(win, &win_width, &win_height);

	// Box containing all canvas items
	double left   = DBL_MAX;
	double right  = DBL_MIN;
	double top    = DBL_MIN;
	double bottom = DBL_MAX;

	FOREACH_ITEM(canvas->impl->_items, i) {
		GanvItem* const item = GANV_ITEM(*i);
		const double    x    = item->x;
		const double    y    = item->y;
		if (GANV_IS_CIRCLE(*i)) {
			const double r = GANV_CIRCLE(*i)->impl->coords.radius;
			left   = MIN(left, x - r);
			right  = MAX(right, x + r);
			bottom = MIN(bottom, y - r);
			top    = MAX(top, y + r);
		} else {
			left   = MIN(left, x);
			right  = MAX(right, x + ganv_box_get_width(GANV_BOX(*i)));
			bottom = MIN(bottom, y);
			top    = MAX(top, y + ganv_box_get_height(GANV_BOX(*i)));
		}
	}

	static const double pad = 8.0;

	const double new_zoom = std::min(
		((double)win_width / (double)(right - left + pad*2.0)),
		((double)win_height / (double)(top - bottom + pad*2.0)));

	ganv_canvas_set_zoom(canvas, new_zoom);

	int scroll_x, scroll_y;
	ganv_canvas_base_w2c(GANV_CANVAS_BASE(canvas->impl->_gcanvas),
	                     lrintf(left - pad), lrintf(bottom - pad),
	                     &scroll_x, &scroll_y);

	ganv_canvas_base_scroll_to(GANV_CANVAS_BASE(canvas->impl->_gcanvas),
	                           scroll_x, scroll_y);
}

static void
set_node_direction(GanvNode* node, void* data)
{
	if (GANV_IS_MODULE(node)) {
		ganv_module_set_direction(GANV_MODULE(node), *(GanvDirection*)data);
	}
}

void
ganv_canvas_set_direction(GanvCanvas* canvas, GanvDirection dir)
{
	if (canvas->direction != dir) {
		canvas->direction = dir;
		ganv_canvas_for_each_node(canvas, set_node_direction, &dir);
		ganv_canvas_contents_changed(canvas);
	}
}

void
ganv_canvas_clear_selection(GanvCanvas* canvas)
{
	canvas->impl->clear_selection();
}

void
ganv_canvas_move_selected_items(GanvCanvas* canvas,
                                double      dx,
                                double      dy)
{
	return canvas->impl->move_selected_items(dx, dy);
}

void
ganv_canvas_selection_move_finished(GanvCanvas* canvas)
{
	return canvas->impl->selection_move_finished();
}

void
ganv_canvas_select_node(GanvCanvas* canvas,
                        GanvNode*   node)
{
	canvas->impl->select_item(node);
}

void
ganv_canvas_unselect_node(GanvCanvas* canvas,
                          GanvNode*   node)
{
	canvas->impl->unselect_item(node);
}

void
ganv_canvas_add_node(GanvCanvas* canvas,
                     GanvNode*   node)
{
	canvas->impl->add_item(node);
}

GanvEdge*
ganv_canvas_get_edge(GanvCanvas* canvas,
                     GanvNode*   tail,
                     GanvNode*   head)
{
	return canvas->impl->get_edge_between(tail, head);
}

void
ganv_canvas_remove_edge_between(GanvCanvas* canvas,
                                GanvNode*   tail,
                                GanvNode*   head)
{
	canvas->impl->remove_edge(canvas->impl->get_edge_between(tail, head));
}

void
ganv_canvas_disconnect_edge(GanvCanvas* canvas,
                            GanvEdge*   edge)
{
	g_signal_emit(canvas, signal_disconnect, 0,
	              edge->impl->tail, edge->impl->head, NULL);
}

void
ganv_canvas_remove_node(GanvCanvas* canvas,
                        GanvNode*   node)
{
	canvas->impl->remove_item(node);
}

void
ganv_canvas_add_edge(GanvCanvas* canvas,
                     GanvEdge*   edge)
{
	canvas->impl->_edges.insert(edge);
	canvas->impl->_dst_edges.insert(edge);
	canvas->impl->contents_changed();
}

void
ganv_canvas_remove_edge(GanvCanvas* canvas,
                        GanvEdge*   edge)
{
	canvas->impl->remove_edge(edge);
}

void
ganv_canvas_select_edge(GanvCanvas* canvas,
                        GanvEdge*   edge)
{
	canvas->impl->select_edge(edge);
}

void
ganv_canvas_unselect_edge(GanvCanvas* canvas,
                          GanvEdge*   edge)
{
	canvas->impl->unselect_edge(edge);
}

void
ganv_canvas_for_each_node(GanvCanvas*  canvas,
                          GanvNodeFunc f,
                          void*        data)
{
	canvas->impl->for_each_node(f, data);
}

void
ganv_canvas_for_each_selected_node(GanvCanvas*  canvas,
                                   GanvNodeFunc f,
                                   void*        data)
{
	FOREACH_ITEM(canvas->impl->_selected_items, i) {
		f(*i, data);
	}
}

gboolean
ganv_canvas_empty(const GanvCanvas* canvas)
{
	return canvas->impl->_items.empty();
}

void
ganv_canvas_for_each_edge_from(GanvCanvas*     canvas,
                               const GanvNode* tail,
                               GanvEdgeFunc    f,
                               void*           data)
{
	canvas->impl->for_each_edge_from(tail, f, data);
}

void
ganv_canvas_for_each_edge_to(GanvCanvas*     canvas,
                             const GanvNode* head,
                             GanvEdgeFunc    f,
                             void*           data)
{
	canvas->impl->for_each_edge_to(head, f, data);
}

void
ganv_canvas_for_each_edge_on(GanvCanvas*     canvas,
                             const GanvNode* node,
                             GanvEdgeFunc    f,
                             void*           data)
{
	canvas->impl->for_each_edge_on(node, f, data);
}

GdkCursor*
ganv_canvas_get_move_cursor(const GanvCanvas* canvas)
{
	return canvas->impl->_move_cursor;
}

gboolean
ganv_canvas_port_event(GanvCanvas* canvas,
                       GanvPort*   port,
                       GdkEvent*   event)
{
	return canvas->impl->port_event(event, port);
}

void
ganv_canvas_clear(GanvCanvas* canvas)
{
	canvas->impl->_selected_items.clear();
	canvas->impl->_selected_edges.clear();

	Items items = canvas->impl->_items; // copy
	FOREACH_ITEM(items, i) {
		gtk_object_destroy(GTK_OBJECT(*i));
	}
	canvas->impl->_items.clear();

	GanvCanvasImpl::Edges edges = canvas->impl->_edges; // copy
	FOREACH_EDGE(edges, i) {
		gtk_object_destroy(GTK_OBJECT(*i));
	}
	canvas->impl->_edges.clear();

	canvas->impl->_selected_ports.clear();
	canvas->impl->_connect_port = NULL;
}

void
ganv_canvas_select_all(GanvCanvas* canvas)
{
	ganv_canvas_clear_selection(canvas);
	FOREACH_ITEM(canvas->impl->_items, m) {
		canvas->impl->select_item(*m);
	}
}

double
ganv_canvas_get_zoom(GanvCanvas* canvas)
{
	return canvas->impl->_zoom;
}

void
ganv_canvas_move_contents_to(GanvCanvas* canvas, double x, double y)
{
	double min_x=HUGE_VAL, min_y=HUGE_VAL;
	FOREACH_ITEM(canvas->impl->_items, i) {
		const double x = GANV_ITEM(*i)->x;
		const double y = GANV_ITEM(*i)->y;
		min_x = std::min(min_x, x);
		min_y = std::min(min_y, y);
	}
	canvas->impl->move_contents_to_internal(x, y, min_x, min_y);
}

void
ganv_canvas_arrange(GanvCanvas* canvas)
{	
#if defined(HAVE_AGRAPH_2_20) || defined(HAVE_AGRAPH_2_30)
	GVNodes nodes = canvas->impl->layout_dot((char*)"");

	double least_x=HUGE_VAL, least_y=HUGE_VAL, most_x=0, most_y=0;

	// Set numeric locale to POSIX for reading graphviz output with strtod
	char* locale = strdup(setlocale(LC_NUMERIC, NULL));
	setlocale(LC_NUMERIC, "POSIX");

	const double dpi = gdk_screen_get_resolution(gdk_screen_get_default());
	const double dpp = dpi / 72.0;

	// Arrange to graphviz coordinates
	for (GVNodes::iterator i = nodes.begin(); i != nodes.end(); ++i) {
		if (GANV_ITEM(i->first)->parent != GANV_ITEM(ganv_canvas_get_root(canvas))) {
			continue;
		}
		const string pos   = agget(i->second, (char*)"pos");
		const string x_str = pos.substr(0, pos.find(","));
		const string y_str = pos.substr(pos.find(",") + 1);
		const double cx    = lrint(strtod(x_str.c_str(), NULL) * dpp);
		const double cy    = lrint(strtod(y_str.c_str(), NULL) * dpp);

		double w, h;
		if (GANV_IS_BOX(i->first)) {
			w = ganv_box_get_width(GANV_BOX(i->first));
			h = ganv_box_get_height(GANV_BOX(i->first));
		} else {
			w = h = ganv_circle_get_radius(GANV_CIRCLE(i->first)) * 2.3;
		}

		/* Dot node positions are supposedly node centers, but things only
		   match up if x is interpreted as center and y as top...
		*/
		const double x = cx - (w / 2.0);
		const double y = -cy - (h / 2.0);

		ganv_node_move_to(i->first, x, y);

		least_x = std::min(least_x, x);
		least_y = std::min(least_y, y);
		most_x  = std::max(most_x, x);
		most_y  = std::max(most_y, y);
	}

	// Reset numeric locale to original value
	setlocale(LC_NUMERIC, locale);
	free(locale);

	const double graph_width  = most_x - least_x;
	const double graph_height = most_y - least_y;

	//cerr << "CWH: " << _width << ", " << _height << endl;
	//cerr << "GWH: " << graph_width << ", " << graph_height << endl;

	double old_width, old_height;
	g_object_get(G_OBJECT(canvas),
	             "width", &old_width,
	             "height", &old_height,
	             NULL);

	const double new_width  = std::max(graph_width + 10.0, old_width);
	const double new_height = std::max(graph_height + 10.0, old_height);
	if (new_width != old_width || new_height != old_height) {
		ganv_canvas_resize(canvas, new_width, new_height);
	}
	nodes.cleanup();

	static const double border_width = canvas->impl->_font_size * 2.0;
	canvas->impl->move_contents_to_internal(border_width, border_width, least_x, least_y);
	ganv_canvas_base_scroll_to(GANV_CANVAS_BASE(canvas->impl->_gcanvas), 0, 0);

	FOREACH_ITEM(canvas->impl->_items, i) {
		const double x = GANV_ITEM(*i)->x;
		const double y = GANV_ITEM(*i)->y;
		g_signal_emit(*i, signal_moved, 0, x, y, NULL);
	}
#endif
}

void
ganv_canvas_export_dot(GanvCanvas* canvas, const char* filename)
{
#if defined(HAVE_AGRAPH_2_20) || defined(HAVE_AGRAPH_2_30)
	GVNodes nodes = canvas->impl->layout_dot(filename);
	nodes.cleanup();
#endif
}

} // extern "C"
