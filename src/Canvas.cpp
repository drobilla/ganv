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

/* Parts based on GnomeCanvas, by Federico Mena <federico@nuclecu.unam.mx>
 * and Raph Levien <raph@gimp.org>
 * Copyright 1997-2000 Free Software Foundation
 */

#define _POSIX_C_SOURCE 200809L // strdup

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <list>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <cairo.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <gtk/gtkstyle.h>
#include <gtkmm/widget.h>

#include "ganv/Canvas.hpp"
#include "ganv/Circle.hpp"
#include "ganv/Edge.hpp"
#include "ganv/Module.hpp"
#include "ganv/Port.hpp"
#include "ganv/box.h"
#include "ganv/canvas.h"
#include "ganv/edge.h"
#include "ganv/group.h"
#include "ganv/node.h"
#include "ganv_config.h"

#include "./color.h"
#include "./ganv-marshal.h"
#include "./ganv-private.h"
#include "./gettext.h"

#if defined(HAVE_AGRAPH_2_20) || defined(HAVE_AGRAPH_2_30)
#    include <gvc.h>
#endif
#ifdef GANV_FDGL
#    include "./fdgl.hpp"
#endif

/*** GanvCanvas ***/

#define CANVAS_IDLE_PRIORITY (GDK_PRIORITY_REDRAW - 5)

typedef struct {
	int x;
	int y;
	int width;
	int height;
} IRect;

extern "C" {
static void add_idle(GanvCanvas* canvas);
static void ganv_canvas_destroy(GtkObject* object);
static void ganv_canvas_map(GtkWidget* widget);
static void ganv_canvas_unmap(GtkWidget* widget);
static void ganv_canvas_realize(GtkWidget* widget);
static void ganv_canvas_unrealize(GtkWidget* widget);
static void ganv_canvas_size_allocate(GtkWidget*     widget,
                                           GtkAllocation* allocation);
static gint ganv_canvas_button(GtkWidget*      widget,
                                    GdkEventButton* event);
static gint ganv_canvas_motion(GtkWidget*      widget,
                                    GdkEventMotion* event);
static gint ganv_canvas_expose(GtkWidget*      widget,
                                    GdkEventExpose* event);
static gboolean ganv_canvas_key(GtkWidget*   widget,
                                     GdkEventKey* event);
static gboolean ganv_canvas_scroll(GtkWidget*      widget,
                                        GdkEventScroll* event);
static gint ganv_canvas_crossing(GtkWidget*        widget,
                                      GdkEventCrossing* event);
static gint ganv_canvas_focus_in(GtkWidget*     widget,
                                      GdkEventFocus* event);
static gint ganv_canvas_focus_out(GtkWidget*     widget,
                                       GdkEventFocus* event);
static void ganv_canvas_request_update_real(GanvCanvas* canvas);

static GtkLayoutClass* canvas_parent_class;
}

/*** end GanvCanvas ***/

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

		g_signal_connect(G_OBJECT(ganv_canvas_root(GANV_CANVAS(_gcanvas))),
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
		return ganv_canvas_root(GANV_CANVAS(_gcanvas));
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
	if (_drag_state == EDGE) {
		return FALSE;  // Canvas is locked, halt layout process
	}

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

		static const float damp = 0.3;  // Velocity damping

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
	GanvItem* item = ganv_canvas_get_item_at(GANV_CANVAS(_gcanvas), x, y);
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
		ganv_canvas_get_scroll_offsets(GANV_CANVAS(_gcanvas), &scroll_x, &scroll_y);
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
			ganv_canvas_scroll_to(GANV_CANVAS(_gcanvas), scroll_x, scroll_y);
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
		ganv_canvas_get_scroll_offsets(GANV_CANVAS(_gcanvas), &original_scroll_x, &original_scroll_y);
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
		ganv_canvas_scroll_to(GANV_CANVAS(_gcanvas),
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
					GANV_ITEM(ganv_canvas_root(GANV_CANVAS(_gcanvas))),
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
		ganv_canvas_set_scroll_region(GANV_CANVAS(_gcanvas),
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
		ganv_canvas_set_pixels_per_unit(GANV_CANVAS(_gcanvas), zoom);
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
	return ganv_canvas_root(GANV_CANVAS(impl()->_gcanvas));
}

Gtk::Layout&
Canvas::widget()
{
	return *Glib::wrap(impl()->_layout);
}

void
Canvas::get_scroll_offsets(int& cx, int& cy) const
{
	ganv_canvas_get_scroll_offsets(GANV_CANVAS(impl()->_gcanvas), &cx, &cy);
}

void
Canvas::scroll_to(int x, int y)
{
	ganv_canvas_scroll_to(GANV_CANVAS(impl()->_gcanvas), x, y);
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

#include "ganv/canvas.h"

#include "./boilerplate.h"
#include "./color.h"
#include "./gettext.h"

G_DEFINE_TYPE(GanvCanvas, ganv_canvas, GTK_TYPE_LAYOUT)

enum {
	PROP_0,
	PROP_WIDTH,
	PROP_HEIGHT,
	PROP_DIRECTION,
	PROP_FONT_SIZE,
	PROP_LOCKED,
	PROP_FOCUSED_ITEM
};

/* Callback used when the root item of a canvas is destroyed.  The user should
 * never ever do this, so we panic if this happens.
 */
static void
panic_root_destroyed(GtkObject* object, gpointer data)
{
	g_error("Eeeek, root item %p of canvas %p was destroyed!", (void*)object, data);
}

static void
ganv_canvas_init(GanvCanvas* canvas)
{
	GTK_WIDGET_SET_FLAGS(canvas, GTK_CAN_FOCUS);

	canvas->need_update   = FALSE;
	canvas->need_redraw   = FALSE;
	canvas->redraw_region = NULL;
	canvas->idle_id       = 0;

	canvas->scroll_x1 = 0.0;
	canvas->scroll_y1 = 0.0;
	canvas->scroll_x2 = canvas->layout.width;
	canvas->scroll_y2 = canvas->layout.height;

	canvas->pixels_per_unit = 1.0;

	canvas->pick_event.type       = GDK_LEAVE_NOTIFY;
	canvas->pick_event.crossing.x = 0;
	canvas->pick_event.crossing.y = 0;

	canvas->center_scroll_region = TRUE;

	gtk_layout_set_hadjustment(GTK_LAYOUT(canvas), NULL);
	gtk_layout_set_vadjustment(GTK_LAYOUT(canvas), NULL);

	/* Create the root item as a special case */
	canvas->root         = GANV_ITEM(g_object_new(ganv_group_get_type(), NULL));
	canvas->root->canvas = canvas;

	g_object_ref_sink(canvas->root);

	canvas->root_destroy_id = g_signal_connect(canvas->root, "destroy",
	                                           G_CALLBACK(panic_root_destroyed),
	                                           canvas);

	canvas->need_repick = TRUE;
	
	/*************************/

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
	case PROP_FOCUSED_ITEM:
		canvas->focused_item = GANV_ITEM(g_value_get_object(value));
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
	case PROP_FOCUSED_ITEM:
		g_value_set_object(value, GANV_CANVAS(object)->focused_item);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
ganv_canvas_class_init(GanvCanvasClass* klass)
{
	GObjectClass*   gobject_class = (GObjectClass*)klass;
	GtkObjectClass* object_class  = (GtkObjectClass*)klass;
	GtkWidgetClass* widget_class  = (GtkWidgetClass*)klass;

	canvas_parent_class = GTK_LAYOUT_CLASS(g_type_class_peek_parent(klass));

	gobject_class->set_property = ganv_canvas_set_property;
	gobject_class->get_property = ganv_canvas_get_property;

	object_class->destroy = ganv_canvas_destroy;

	widget_class->map                  = ganv_canvas_map;
	widget_class->unmap                = ganv_canvas_unmap;
	widget_class->realize              = ganv_canvas_realize;
	widget_class->unrealize            = ganv_canvas_unrealize;
	widget_class->size_allocate        = ganv_canvas_size_allocate;
	widget_class->button_press_event   = ganv_canvas_button;
	widget_class->button_release_event = ganv_canvas_button;
	widget_class->motion_notify_event  = ganv_canvas_motion;
	widget_class->expose_event         = ganv_canvas_expose;
	widget_class->key_press_event      = ganv_canvas_key;
	widget_class->key_release_event    = ganv_canvas_key;
	widget_class->enter_notify_event   = ganv_canvas_crossing;
	widget_class->leave_notify_event   = ganv_canvas_crossing;
	widget_class->focus_in_event       = ganv_canvas_focus_in;
	widget_class->focus_out_event      = ganv_canvas_focus_out;
	widget_class->scroll_event         = ganv_canvas_scroll;

	klass->request_update = ganv_canvas_request_update_real;

	g_object_class_install_property(
		gobject_class, PROP_FOCUSED_ITEM, g_param_spec_object(
			"focused-item",
			NULL,
			NULL,
			GANV_TYPE_ITEM,
			(GParamFlags)(G_PARAM_READABLE | G_PARAM_WRITABLE)));

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

/**
 * ganv_canvas_resize:
 * Resize the canvas to the given dimensions.
 */
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

/**
 * ganv_canvas_get_default_font_size:
 *
 * Get the default font size in points.
 */
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
		GTK_WIDGET(GANV_CANVAS(canvas->impl->_gcanvas)));
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
	ganv_canvas_w2c(GANV_CANVAS(canvas->impl->_gcanvas),
	                     lrintf(left - pad), lrintf(bottom - pad),
	                     &scroll_x, &scroll_y);

	ganv_canvas_scroll_to(GANV_CANVAS(canvas->impl->_gcanvas),
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

/**
 * ganv_canvas_get_edge:
 * Get the edge between two nodes, or NULL if none exists.
 *
 * Return value: (transfer none): The root group of @canvas.
 */
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
		if (GANV_ITEM(i->first)->parent != GANV_ITEM(ganv_canvas_root(canvas))) {
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
	ganv_canvas_scroll_to(GANV_CANVAS(canvas->impl->_gcanvas), 0, 0);

	FOREACH_ITEM(canvas->impl->_items, i) {
		const double x = GANV_ITEM(*i)->x;
		const double y = GANV_ITEM(*i)->y;
		g_signal_emit(*i, signal_moved, 0, x, y, NULL);
	}
#endif
}

/**
 * ganv_canvas_export_dot:
 *
 * Write a Graphviz DOT description of the canvas to a file.
 */
void
ganv_canvas_export_dot(GanvCanvas* canvas, const char* filename)
{
#if defined(HAVE_AGRAPH_2_20) || defined(HAVE_AGRAPH_2_30)
	GVNodes nodes = canvas->impl->layout_dot(filename);
	nodes.cleanup();
#endif
}


/***************************************************************************/

/* Convenience function to remove the idle handler of a canvas */
static void
remove_idle(GanvCanvas* canvas)
{
	if (canvas->idle_id == 0) {
		return;
	}

	g_source_remove(canvas->idle_id);
	canvas->idle_id = 0;
}

/* Removes the transient state of the canvas (idle handler, grabs). */
static void
shutdown_transients(GanvCanvas* canvas)
{
	/* We turn off the need_redraw flag, since if the canvas is mapped again
	 * it will request a redraw anyways.  We do not turn off the need_update
	 * flag, though, because updates are not queued when the canvas remaps
	 * itself.
	 */
	if (canvas->need_redraw) {
		canvas->need_redraw = FALSE;
		g_slist_foreach(canvas->redraw_region, (GFunc)g_free, NULL);
		g_slist_free(canvas->redraw_region);
		canvas->redraw_region = NULL;
		canvas->redraw_x1   = 0;
		canvas->redraw_y1   = 0;
		canvas->redraw_x2   = 0;
		canvas->redraw_y2   = 0;
	}

	if (canvas->grabbed_item) {
		canvas->grabbed_item = NULL;
		gdk_pointer_ungrab(GDK_CURRENT_TIME);
	}

	remove_idle(canvas);
}

/* Destroy handler for GanvCanvas */
static void
ganv_canvas_destroy(GtkObject* object)
{
	GanvCanvas* canvas;

	g_return_if_fail(GANV_IS_CANVAS(object));

	/* remember, destroy can be run multiple times! */

	canvas = GANV_CANVAS(object);

	if (canvas->root_destroy_id) {
		g_signal_handler_disconnect(canvas->root, canvas->root_destroy_id);
		canvas->root_destroy_id = 0;
	}
	if (canvas->root) {
		gtk_object_destroy(GTK_OBJECT(canvas->root));
		g_object_unref(G_OBJECT(canvas->root));
		canvas->root = NULL;
	}

	shutdown_transients(canvas);

	if (GTK_OBJECT_CLASS(canvas_parent_class)->destroy) {
		(*GTK_OBJECT_CLASS(canvas_parent_class)->destroy)(object);
	}
}

/**
 * ganv_canvas_new:
 *
 * Return value: A newly-created canvas.
 */
GanvCanvas*
ganv_canvas_new(double width, double height)
{
	GanvCanvas* canvas = GANV_CANVAS(
		g_object_new(ganv_canvas_get_type(),
		             "width", width,
		             "height", height,
		             NULL));

	ganv_canvas_set_scroll_region(GANV_CANVAS(canvas),
	                                   0.0, 0.0, width, height);

	return canvas;
}

/* Map handler for the canvas */
static void
ganv_canvas_map(GtkWidget* widget)
{
	GanvCanvas* canvas;

	g_return_if_fail(GANV_IS_CANVAS(widget));

	/* Normal widget mapping stuff */

	if (GTK_WIDGET_CLASS(canvas_parent_class)->map) {
		(*GTK_WIDGET_CLASS(canvas_parent_class)->map)(widget);
	}

	canvas = GANV_CANVAS(widget);

	if (canvas->need_update) {
		add_idle(canvas);
	}

	/* Map items */

	if (GANV_ITEM_GET_CLASS(canvas->root)->map) {
		(*GANV_ITEM_GET_CLASS(canvas->root)->map)(canvas->root);
	}
}

/* Unmap handler for the canvas */
static void
ganv_canvas_unmap(GtkWidget* widget)
{
	GanvCanvas* canvas;

	g_return_if_fail(GANV_IS_CANVAS(widget));

	canvas = GANV_CANVAS(widget);

	shutdown_transients(canvas);

	/* Unmap items */

	if (GANV_ITEM_GET_CLASS(canvas->root)->unmap) {
		(*GANV_ITEM_GET_CLASS(canvas->root)->unmap)(canvas->root);
	}

	/* Normal widget unmapping stuff */

	if (GTK_WIDGET_CLASS(canvas_parent_class)->unmap) {
		(*GTK_WIDGET_CLASS(canvas_parent_class)->unmap)(widget);
	}
}

/* Realize handler for the canvas */
static void
ganv_canvas_realize(GtkWidget* widget)
{
	GanvCanvas* canvas;

	g_return_if_fail(GANV_IS_CANVAS(widget));

	/* Normal widget realization stuff */

	if (GTK_WIDGET_CLASS(canvas_parent_class)->realize) {
		(*GTK_WIDGET_CLASS(canvas_parent_class)->realize)(widget);
	}

	canvas = GANV_CANVAS(widget);

	gdk_window_set_events(
		canvas->layout.bin_window,
		(GdkEventMask)(gdk_window_get_events(canvas->layout.bin_window)
		               | GDK_EXPOSURE_MASK
		               | GDK_BUTTON_PRESS_MASK
		               | GDK_BUTTON_RELEASE_MASK
		               | GDK_POINTER_MOTION_MASK
		               | GDK_KEY_PRESS_MASK
		               | GDK_KEY_RELEASE_MASK
		               | GDK_ENTER_NOTIFY_MASK
		               | GDK_LEAVE_NOTIFY_MASK
		               | GDK_FOCUS_CHANGE_MASK));

	/* Create our own temporary pixmap gc and realize all the items */

	canvas->pixmap_gc = gdk_gc_new(canvas->layout.bin_window);

	(*GANV_ITEM_GET_CLASS(canvas->root)->realize)(canvas->root);
}

/* Unrealize handler for the canvas */
static void
ganv_canvas_unrealize(GtkWidget* widget)
{
	GanvCanvas* canvas;

	g_return_if_fail(GANV_IS_CANVAS(widget));

	canvas = GANV_CANVAS(widget);

	shutdown_transients(canvas);

	/* Unrealize items and parent widget */

	(*GANV_ITEM_GET_CLASS(canvas->root)->unrealize)(canvas->root);

	g_object_unref(canvas->pixmap_gc);
	canvas->pixmap_gc = NULL;

	if (GTK_WIDGET_CLASS(canvas_parent_class)->unrealize) {
		(*GTK_WIDGET_CLASS(canvas_parent_class)->unrealize)(widget);
	}
}

/* Handles scrolling of the canvas.  Adjusts the scrolling and zooming offset to
 * keep as much as possible of the canvas scrolling region in view.
 */
static void
scroll_to(GanvCanvas* canvas, int cx, int cy)
{
	int scroll_width, scroll_height;
	int right_limit, bottom_limit;
	int old_zoom_xofs, old_zoom_yofs;
	int changed_x = FALSE, changed_y = FALSE;
	int canvas_width, canvas_height;

	canvas_width  = GTK_WIDGET(canvas)->allocation.width;
	canvas_height = GTK_WIDGET(canvas)->allocation.height;

	scroll_width = floor((canvas->scroll_x2 - canvas->scroll_x1) * canvas->pixels_per_unit
	                     + 0.5);
	scroll_height = floor((canvas->scroll_y2 - canvas->scroll_y1) * canvas->pixels_per_unit
	                      + 0.5);

	right_limit  = scroll_width - canvas_width;
	bottom_limit = scroll_height - canvas_height;

	old_zoom_xofs = canvas->zoom_xofs;
	old_zoom_yofs = canvas->zoom_yofs;

	if (right_limit < 0) {
		cx = 0;

		if (canvas->center_scroll_region) {
			canvas->zoom_xofs = (canvas_width - scroll_width) / 2;
			scroll_width      = canvas_width;
		} else {
			canvas->zoom_xofs = 0;
		}
	} else if (cx < 0) {
		cx                = 0;
		canvas->zoom_xofs = 0;
	} else if (cx > right_limit) {
		cx                = right_limit;
		canvas->zoom_xofs = 0;
	} else {
		canvas->zoom_xofs = 0;
	}

	if (bottom_limit < 0) {
		cy = 0;

		if (canvas->center_scroll_region) {
			canvas->zoom_yofs = (canvas_height - scroll_height) / 2;
			scroll_height     = canvas_height;
		} else {
			canvas->zoom_yofs = 0;
		}
	} else if (cy < 0) {
		cy                = 0;
		canvas->zoom_yofs = 0;
	} else if (cy > bottom_limit) {
		cy                = bottom_limit;
		canvas->zoom_yofs = 0;
	} else {
		canvas->zoom_yofs = 0;
	}

	if ((canvas->zoom_xofs != old_zoom_xofs) || (canvas->zoom_yofs != old_zoom_yofs)) {
		ganv_canvas_request_update(canvas);
		gtk_widget_queue_draw(GTK_WIDGET(canvas));
	}

	if (canvas->layout.hadjustment && ( ((int)canvas->layout.hadjustment->value) != cx) ) {
		canvas->layout.hadjustment->value = cx;
		changed_x                         = TRUE;
	}

	if (canvas->layout.vadjustment && ( ((int)canvas->layout.vadjustment->value) != cy) ) {
		canvas->layout.vadjustment->value = cy;
		changed_y                         = TRUE;
	}

	if ((scroll_width != (int)canvas->layout.width)
	    || (scroll_height != (int)canvas->layout.height)) {
		gtk_layout_set_size(GTK_LAYOUT(canvas), scroll_width, scroll_height);
	}

	/* Signal GtkLayout that it should do a redraw. */

	if (changed_x) {
		g_signal_emit_by_name(canvas->layout.hadjustment, "value_changed");
	}

	if (changed_y) {
		g_signal_emit_by_name(canvas->layout.vadjustment, "value_changed");
	}
}

/* Size allocation handler for the canvas */
static void
ganv_canvas_size_allocate(GtkWidget* widget, GtkAllocation* allocation)
{
	GanvCanvas* canvas;

	g_return_if_fail(GANV_IS_CANVAS(widget));
	g_return_if_fail(allocation != NULL);

	if (GTK_WIDGET_CLASS(canvas_parent_class)->size_allocate) {
		(*GTK_WIDGET_CLASS(canvas_parent_class)->size_allocate)(widget, allocation);
	}

	canvas = GANV_CANVAS(widget);

	/* Recenter the view, if appropriate */

	canvas->layout.hadjustment->page_size      = allocation->width;
	canvas->layout.hadjustment->page_increment = allocation->width / 2;

	canvas->layout.vadjustment->page_size      = allocation->height;
	canvas->layout.vadjustment->page_increment = allocation->height / 2;

	scroll_to(canvas,
	          canvas->layout.hadjustment->value,
	          canvas->layout.vadjustment->value);

	g_signal_emit_by_name(canvas->layout.hadjustment, "changed");
	g_signal_emit_by_name(canvas->layout.vadjustment, "changed");
}

/* Returns whether the item is an inferior of or is equal to the parent. */
static gboolean
is_descendant(GanvItem* item, GanvItem* parent)
{
	for (; item; item = item->parent) {
		if (item == parent) {
			return TRUE;
		}
	}

	return FALSE;
}


/* Emits an event for an item in the canvas, be it the current item, grabbed
 * item, or focused item, as appropriate.
 */
int
ganv_canvas_emit_event(GanvCanvas* canvas, GdkEvent* event)
{
	GdkEvent* ev;
	gint      finished;
	GanvItem* item;
	GanvItem* parent;
	guint     mask;

	/* Perform checks for grabbed items */

	if (canvas->grabbed_item
	    && !is_descendant(canvas->current_item, canvas->grabbed_item)) {
		/* I think this warning is annoying and I don't know what it's for
		 * so I'll disable it for now.
		 */
		/*                g_warning ("emit_event() returning FALSE!\n");*/
		return FALSE;
	}

	if (canvas->grabbed_item) {
		switch (event->type) {
		case GDK_ENTER_NOTIFY:
			mask = GDK_ENTER_NOTIFY_MASK;
			break;

		case GDK_LEAVE_NOTIFY:
			mask = GDK_LEAVE_NOTIFY_MASK;
			break;

		case GDK_MOTION_NOTIFY:
			mask = GDK_POINTER_MOTION_MASK;
			break;

		case GDK_BUTTON_PRESS:
		case GDK_2BUTTON_PRESS:
		case GDK_3BUTTON_PRESS:
			mask = GDK_BUTTON_PRESS_MASK;
			break;

		case GDK_BUTTON_RELEASE:
			mask = GDK_BUTTON_RELEASE_MASK;
			break;

		case GDK_KEY_PRESS:
			mask = GDK_KEY_PRESS_MASK;
			break;

		case GDK_KEY_RELEASE:
			mask = GDK_KEY_RELEASE_MASK;
			break;

		case GDK_SCROLL:
			mask = GDK_SCROLL_MASK;
			break;

		default:
			mask = 0;
			break;
		}

		if (!(mask & canvas->grabbed_event_mask)) {
			return FALSE;
		}
	}

	/* Convert to world coordinates -- we have two cases because of diferent
	 * offsets of the fields in the event structures.
	 */

	ev = gdk_event_copy(event);

	switch (ev->type) {
	case GDK_ENTER_NOTIFY:
	case GDK_LEAVE_NOTIFY:
		ganv_canvas_window_to_world(canvas,
		                                 ev->crossing.x, ev->crossing.y,
		                                 &ev->crossing.x, &ev->crossing.y);
		break;

	case GDK_MOTION_NOTIFY:
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
	case GDK_BUTTON_RELEASE:
		ganv_canvas_window_to_world(canvas,
		                                 ev->motion.x, ev->motion.y,
		                                 &ev->motion.x, &ev->motion.y);
		break;

	default:
		break;
	}

	/* Choose where we send the event */

	item = canvas->current_item;

	if (canvas->focused_item
	    && ((event->type == GDK_KEY_PRESS)
	        || (event->type == GDK_KEY_RELEASE)
	        || (event->type == GDK_FOCUS_CHANGE))) {
		item = canvas->focused_item;
	}

	/* The event is propagated up the hierarchy (for if someone connected to
	 * a group instead of a leaf event), and emission is stopped if a
	 * handler returns TRUE, just like for GtkWidget events.
	 */

	finished = FALSE;

	while (item && !finished) {
		g_object_ref(G_OBJECT(item));

		ganv_item_emit_event(item, ev, &finished);

		parent = item->parent;
		g_object_unref(G_OBJECT(item));

		item = parent;
	}

	gdk_event_free(ev);

	return finished;
}

/* Re-picks the current item in the canvas, based on the event's coordinates.
 * Also emits enter/leave events for items as appropriate.
 */
static int
pick_current_item(GanvCanvas* canvas, GdkEvent* event)
{
	int    button_down;
	double x, y;
	int    cx, cy;
	int    retval;

	retval = FALSE;

	/* If a button is down, we'll perform enter and leave events on the
	 * current item, but not enter on any other item.  This is more or less
	 * like X pointer grabbing for canvas items.
	 */
	button_down = canvas->state & (GDK_BUTTON1_MASK
	                               | GDK_BUTTON2_MASK
	                               | GDK_BUTTON3_MASK
	                               | GDK_BUTTON4_MASK
	                               | GDK_BUTTON5_MASK);
	if (!button_down) {
		canvas->left_grabbed_item = FALSE;
	}

	/* Save the event in the canvas.  This is used to synthesize enter and
	 * leave events in case the current item changes.  It is also used to
	 * re-pick the current item if the current one gets deleted.  Also,
	 * synthesize an enter event.
	 */
	if (event != &canvas->pick_event) {
		if ((event->type == GDK_MOTION_NOTIFY) || (event->type == GDK_BUTTON_RELEASE)) {
			/* these fields have the same offsets in both types of events */

			canvas->pick_event.crossing.type       = GDK_ENTER_NOTIFY;
			canvas->pick_event.crossing.window     = event->motion.window;
			canvas->pick_event.crossing.send_event = event->motion.send_event;
			canvas->pick_event.crossing.subwindow  = NULL;
			canvas->pick_event.crossing.x          = event->motion.x;
			canvas->pick_event.crossing.y          = event->motion.y;
			canvas->pick_event.crossing.mode       = GDK_CROSSING_NORMAL;
			canvas->pick_event.crossing.detail     = GDK_NOTIFY_NONLINEAR;
			canvas->pick_event.crossing.focus      = FALSE;
			canvas->pick_event.crossing.state      = event->motion.state;

			/* these fields don't have the same offsets in both types of events */

			if (event->type == GDK_MOTION_NOTIFY) {
				canvas->pick_event.crossing.x_root = event->motion.x_root;
				canvas->pick_event.crossing.y_root = event->motion.y_root;
			} else {
				canvas->pick_event.crossing.x_root = event->button.x_root;
				canvas->pick_event.crossing.y_root = event->button.y_root;
			}
		} else {
			canvas->pick_event = *event;
		}
	}

	/* Don't do anything else if this is a recursive call */

	if (canvas->in_repick) {
		return retval;
	}

	/* LeaveNotify means that there is no current item, so we don't look for one */

	if (canvas->pick_event.type != GDK_LEAVE_NOTIFY) {
		/* these fields don't have the same offsets in both types of events */

		if (canvas->pick_event.type == GDK_ENTER_NOTIFY) {
			x = canvas->pick_event.crossing.x - canvas->zoom_xofs;
			y = canvas->pick_event.crossing.y - canvas->zoom_yofs;
		} else {
			x = canvas->pick_event.motion.x - canvas->zoom_xofs;
			y = canvas->pick_event.motion.y - canvas->zoom_yofs;
		}

		/* canvas pixel coords */

		cx = (int)(x + 0.5);
		cy = (int)(y + 0.5);

		/* world coords */

		x = canvas->scroll_x1 + x / canvas->pixels_per_unit;
		y = canvas->scroll_y1 + y / canvas->pixels_per_unit;

		/* find the closest item */

		if (canvas->root->object.flags & GANV_ITEM_VISIBLE) {
			GANV_ITEM_GET_CLASS(canvas->root)->point(
				canvas->root,
				x - canvas->root->x, y - canvas->root->y,
				cx, cy,
				&canvas->new_current_item);
		} else {
			canvas->new_current_item = NULL;
		}
	} else {
		canvas->new_current_item = NULL;
	}

	if ((canvas->new_current_item == canvas->current_item) && !canvas->left_grabbed_item) {
		return retval; /* current item did not change */

	}
	/* Synthesize events for old and new current items */

	if ((canvas->new_current_item != canvas->current_item)
	    && (canvas->current_item != NULL)
	    && !canvas->left_grabbed_item) {
		GdkEvent new_event;

		new_event      = canvas->pick_event;
		new_event.type = GDK_LEAVE_NOTIFY;

		new_event.crossing.detail    = GDK_NOTIFY_ANCESTOR;
		new_event.crossing.subwindow = NULL;
		canvas->in_repick            = TRUE;
		retval                       = ganv_canvas_emit_event(canvas, &new_event);
		canvas->in_repick            = FALSE;
	}

	/* new_current_item may have been set to NULL during the call to ganv_canvas_emit_event() above */

	if ((canvas->new_current_item != canvas->current_item) && button_down) {
		canvas->left_grabbed_item = TRUE;
		return retval;
	}

	/* Handle the rest of cases */

	canvas->left_grabbed_item = FALSE;
	canvas->current_item      = canvas->new_current_item;

	if (canvas->current_item != NULL) {
		GdkEvent new_event;

		new_event                    = canvas->pick_event;
		new_event.type               = GDK_ENTER_NOTIFY;
		new_event.crossing.detail    = GDK_NOTIFY_ANCESTOR;
		new_event.crossing.subwindow = NULL;
		retval                       = ganv_canvas_emit_event(canvas, &new_event);
	}

	return retval;
}

/* Button event handler for the canvas */
static gint
ganv_canvas_button(GtkWidget* widget, GdkEventButton* event)
{
	GanvCanvas* canvas;
	int             mask;
	int             retval;

	g_return_val_if_fail(GANV_IS_CANVAS(widget), FALSE);
	g_return_val_if_fail(event != NULL, FALSE);

	retval = FALSE;

	canvas = GANV_CANVAS(widget);

	/*
	 * dispatch normally regardless of the event's window if an item has
	 * has a pointer grab in effect
	 */
	if (!canvas->grabbed_item && ( event->window != canvas->layout.bin_window) ) {
		return retval;
	}

	switch (event->button) {
	case 1:
		mask = GDK_BUTTON1_MASK;
		break;
	case 2:
		mask = GDK_BUTTON2_MASK;
		break;
	case 3:
		mask = GDK_BUTTON3_MASK;
		break;
	case 4:
		mask = GDK_BUTTON4_MASK;
		break;
	case 5:
		mask = GDK_BUTTON5_MASK;
		break;
	default:
		mask = 0;
	}

	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		/* Pick the current item as if the button were not pressed, and
		 * then process the event.
		 */
		canvas->state = event->state;
		pick_current_item(canvas, (GdkEvent*)event);
		canvas->state ^= mask;
		retval         = ganv_canvas_emit_event(canvas, (GdkEvent*)event);
		break;

	case GDK_BUTTON_RELEASE:
		/* Process the event as if the button were pressed, then repick
		 * after the button has been released
		 */
		canvas->state = event->state;
		retval        = ganv_canvas_emit_event(canvas, (GdkEvent*)event);
		event->state ^= mask;
		canvas->state = event->state;
		pick_current_item(canvas, (GdkEvent*)event);
		event->state ^= mask;
		break;

	default:
		g_assert_not_reached();
	}

	return retval;
}

/* Motion event handler for the canvas */
static gint
ganv_canvas_motion(GtkWidget* widget, GdkEventMotion* event)
{
	GanvCanvas* canvas;

	g_return_val_if_fail(GANV_IS_CANVAS(widget), FALSE);
	g_return_val_if_fail(event != NULL, FALSE);

	canvas = GANV_CANVAS(widget);

	if (event->window != canvas->layout.bin_window) {
		return FALSE;
	}

	canvas->state = event->state;
	pick_current_item(canvas, (GdkEvent*)event);
	return ganv_canvas_emit_event(canvas, (GdkEvent*)event);
}

static gboolean
ganv_canvas_scroll(GtkWidget* widget, GdkEventScroll* event)
{
	GanvCanvas* canvas;

	g_return_val_if_fail(GANV_IS_CANVAS(widget), FALSE);
	g_return_val_if_fail(event != NULL, FALSE);

	canvas = GANV_CANVAS(widget);

	if (event->window != canvas->layout.bin_window) {
		return FALSE;
	}

	canvas->state = event->state;
	pick_current_item(canvas, (GdkEvent*)event);
	return ganv_canvas_emit_event(canvas, (GdkEvent*)event);
}

/* Key event handler for the canvas */
static gboolean
ganv_canvas_key(GtkWidget* widget, GdkEventKey* event)
{
	GanvCanvas* canvas;

	g_return_val_if_fail(GANV_IS_CANVAS(widget), FALSE);
	g_return_val_if_fail(event != NULL, FALSE);

	canvas = GANV_CANVAS(widget);

	if (!ganv_canvas_emit_event(canvas, (GdkEvent*)event)) {
		GtkWidgetClass* widget_class;

		widget_class = GTK_WIDGET_CLASS(canvas_parent_class);

		if (event->type == GDK_KEY_PRESS) {
			if (widget_class->key_press_event) {
				return (*widget_class->key_press_event)(widget, event);
			}
		} else if (event->type == GDK_KEY_RELEASE) {
			if (widget_class->key_release_event) {
				return (*widget_class->key_release_event)(widget, event);
			}
		} else {
			g_assert_not_reached();
		}

		return FALSE;
	} else {
		return TRUE;
	}
}

/* Crossing event handler for the canvas */
static gint
ganv_canvas_crossing(GtkWidget* widget, GdkEventCrossing* event)
{
	GanvCanvas* canvas;

	g_return_val_if_fail(GANV_IS_CANVAS(widget), FALSE);
	g_return_val_if_fail(event != NULL, FALSE);

	canvas = GANV_CANVAS(widget);

	if (event->window != canvas->layout.bin_window) {
		return FALSE;
	}

	canvas->state = event->state;
	return pick_current_item(canvas, (GdkEvent*)event);
}

/* Focus in handler for the canvas */
static gint
ganv_canvas_focus_in(GtkWidget* widget, GdkEventFocus* event)
{
	GanvCanvas* canvas;

	GTK_WIDGET_SET_FLAGS(widget, GTK_HAS_FOCUS);

	canvas = GANV_CANVAS(widget);

	if (canvas->focused_item) {
		return ganv_canvas_emit_event(canvas, (GdkEvent*)event);
	} else {
		return FALSE;
	}
}

/* Focus out handler for the canvas */
static gint
ganv_canvas_focus_out(GtkWidget* widget, GdkEventFocus* event)
{
	GanvCanvas* canvas;

	GTK_WIDGET_UNSET_FLAGS(widget, GTK_HAS_FOCUS);

	canvas = GANV_CANVAS(widget);

	if (canvas->focused_item) {
		return ganv_canvas_emit_event(canvas, (GdkEvent*)event);
	} else {
		return FALSE;
	}
}

#define REDRAW_QUANTUM_SIZE 512

static void
ganv_canvas_paint_rect(GanvCanvas* canvas, gint x0, gint y0, gint x1, gint y1)
{
	gint draw_x1, draw_y1;
	gint draw_x2, draw_y2;
	gint draw_width, draw_height;

	g_return_if_fail(!canvas->need_update);

	draw_x1 = MAX(x0, canvas->layout.hadjustment->value - canvas->zoom_xofs);
	draw_y1 = MAX(y0, canvas->layout.vadjustment->value - canvas->zoom_yofs);
	draw_x2 = MIN(draw_x1 + GTK_WIDGET(canvas)->allocation.width, x1);
	draw_y2 = MIN(draw_y1 + GTK_WIDGET(canvas)->allocation.height, y1);

	draw_width  = draw_x2 - draw_x1;
	draw_height = draw_y2 - draw_y1;

	if ((draw_width < 1) || (draw_height < 1)) {
		return;
	}

	canvas->redraw_x1 = draw_x1;
	canvas->redraw_y1 = draw_y1;
	canvas->redraw_x2 = draw_x2;
	canvas->redraw_y2 = draw_y2;
	canvas->draw_xofs = draw_x1;
	canvas->draw_yofs = draw_y1;

	cairo_t* cr = gdk_cairo_create(canvas->layout.bin_window);

	double wx, wy;
	ganv_canvas_window_to_world(canvas, 0, 0, &wx, &wy);
	cairo_translate(cr, -wx, -wy);

	if (canvas->root->object.flags & GANV_ITEM_VISIBLE) {
		(*GANV_ITEM_GET_CLASS(canvas->root)->draw)(
		    canvas->root, cr,
		    draw_x1, draw_y1,
		    draw_width, draw_height);
	}

	cairo_destroy(cr);
}

/* Expose handler for the canvas */
static gint
ganv_canvas_expose(GtkWidget* widget, GdkEventExpose* event)
{
	GanvCanvas* canvas = GANV_CANVAS(widget);
	if (!GTK_WIDGET_DRAWABLE(widget) ||
	    (event->window != canvas->layout.bin_window)) {
		return FALSE;
	}

	/* Find a single bounding rectangle for all rectangles in the region.
	   Since drawing the root group is O(n) and thus very expensive for large
	   canvases, it's much faster to do a single paint than many, even though
	   more area may be painted that way.  With a better group implementation,
	   it would likely be better to paint each changed rectangle separately. */
	GdkRectangle clip;
	gdk_region_get_clipbox(event->region, &clip);

	const int x2 = clip.x + clip.width;
	const int y2 = clip.y + clip.height;
	
	if (canvas->need_update || canvas->need_redraw) {
		/* Update or drawing is scheduled, so just mark exposed area as dirty */
		ganv_canvas_request_redraw(canvas, clip.x, clip.y, x2, y2);
	} else {
		/* No pending updates, draw exposed area immediately */
		ganv_canvas_paint_rect(canvas, clip.x, clip.y, x2, y2);

		/* And call expose on parent container class */
		if (GTK_WIDGET_CLASS(canvas_parent_class)->expose_event) {
			(*GTK_WIDGET_CLASS(canvas_parent_class)->expose_event)(
				widget, event);
		}
	}

	return FALSE;
}

/* Repaints the areas in the canvas that need it */
static void
paint(GanvCanvas* canvas)
{
	for (GSList* l = canvas->redraw_region; l; l = l->next) {
		IRect* rect = (IRect*)l->data;

		const GdkRectangle gdkrect = {
			rect->x + canvas->zoom_xofs,
			rect->y + canvas->zoom_yofs,
			rect->width,
			rect->height
		};

		gdk_window_invalidate_rect(canvas->layout.bin_window, &gdkrect, FALSE);
		g_free(rect);
	}

	g_slist_free(canvas->redraw_region);
	canvas->redraw_region = NULL;
	canvas->need_redraw = FALSE;

	canvas->redraw_x1 = 0;
	canvas->redraw_y1 = 0;
	canvas->redraw_x2 = 0;
	canvas->redraw_y2 = 0;
}

static void
do_update(GanvCanvas* canvas)
{
	/* Cause the update if necessary */

update_again:
	if (canvas->need_update) {
		ganv_item_invoke_update(canvas->root, 0);

		canvas->need_update = FALSE;
	}

	/* Pick new current item */

	while (canvas->need_repick) {
		canvas->need_repick = FALSE;
		pick_current_item(canvas, &canvas->pick_event);
	}

	/* it is possible that during picking we emitted an event in which
	   the user then called some function which then requested update
	   of something.  Without this we'd be left in a state where
	   need_update would have been left TRUE and the canvas would have
	   been left unpainted. */
	if (canvas->need_update) {
		goto update_again;
	}

	/* Paint if able to */

	if (GTK_WIDGET_DRAWABLE(canvas) && canvas->need_redraw) {
		paint(canvas);
	}
}

/* Idle handler for the canvas.  It deals with pending updates and redraws. */
static gboolean
idle_handler(gpointer data)
{
	GanvCanvas* canvas;

	GDK_THREADS_ENTER();

	canvas = GANV_CANVAS(data);

	do_update(canvas);

	/* Reset idle id */
	canvas->idle_id = 0;

	GDK_THREADS_LEAVE();

	return FALSE;
}

/* Convenience function to add an idle handler to a canvas */
static void
add_idle(GanvCanvas* canvas)
{
	g_assert(canvas->need_update || canvas->need_redraw);

	if (!canvas->idle_id) {
		canvas->idle_id = g_idle_add_full(CANVAS_IDLE_PRIORITY,
		                                  idle_handler,
		                                  canvas,
		                                  NULL);
	}

	/*      canvas->idle_id = gtk_idle_add (idle_handler, canvas); */
}

/**
 * ganv_canvas_root:
 * @canvas: A canvas.
 *
 * Return value: (transfer none): The root group of the canvas.
 */
GanvItem*
ganv_canvas_root(GanvCanvas* canvas)
{
	g_return_val_if_fail(GANV_IS_CANVAS(canvas), NULL);

	return canvas->root;
}

/**
 * ganv_canvas_set_scroll_region:
 * @canvas: A canvas.
 * @x1: Leftmost limit of the scrolling region.
 * @y1: Upper limit of the scrolling region.
 * @x2: Rightmost limit of the scrolling region.
 * @y2: Lower limit of the scrolling region.
 *
 * Sets the scrolling region of a canvas to the specified rectangle.  The canvas
 * will then be able to scroll only within this region.  The view of the canvas
 * is adjusted as appropriate to display as much of the new region as possible.
 **/
void
ganv_canvas_set_scroll_region(GanvCanvas* canvas, double x1, double y1, double x2,
                                   double y2)
{
	double wxofs, wyofs;
	int    xofs, yofs;

	g_return_if_fail(GANV_IS_CANVAS(canvas));

	/*
	 * Set the new scrolling region.  If possible, do not move the visible contents of the
	 * canvas.
	 */

	ganv_canvas_c2w(canvas,
	                     GTK_LAYOUT(canvas)->hadjustment->value + canvas->zoom_xofs,
	                     GTK_LAYOUT(canvas)->vadjustment->value + canvas->zoom_yofs,
	                     /*canvas->zoom_xofs,
	                        canvas->zoom_yofs,*/
	                     &wxofs, &wyofs);

	canvas->scroll_x1 = x1;
	canvas->scroll_y1 = y1;
	canvas->scroll_x2 = x2;
	canvas->scroll_y2 = y2;

	ganv_canvas_w2c(canvas, wxofs, wyofs, &xofs, &yofs);

	scroll_to(canvas, xofs, yofs);

	canvas->need_repick = TRUE;
#if 0
	/* todo: should be requesting update */
	(*GANV_ITEM_CLASS(canvas->root->object.klass)->update)(
	    canvas->root, NULL, NULL, 0);
#endif
}

/**
 * ganv_canvas_get_scroll_region:
 * @canvas: A canvas.
 * @x1: Leftmost limit of the scrolling region (return value).
 * @y1: Upper limit of the scrolling region (return value).
 * @x2: Rightmost limit of the scrolling region (return value).
 * @y2: Lower limit of the scrolling region (return value).
 *
 * Queries the scrolling region of a canvas.
 **/
void
ganv_canvas_get_scroll_region(GanvCanvas* canvas, double* x1, double* y1, double* x2,
                                   double* y2)
{
	g_return_if_fail(GANV_IS_CANVAS(canvas));

	if (x1) {
		*x1 = canvas->scroll_x1;
	}

	if (y1) {
		*y1 = canvas->scroll_y1;
	}

	if (x2) {
		*x2 = canvas->scroll_x2;
	}

	if (y2) {
		*y2 = canvas->scroll_y2;
	}
}

/**
 * ganv_canvas_set_center_scroll_region:
 * @canvas: A canvas.
 * @center_scroll_region: Whether to center the scrolling region in the canvas
 * window when it is smaller than the canvas' allocation.
 *
 * When the scrolling region of the canvas is smaller than the canvas window,
 * e.g.  the allocation of the canvas, it can be either centered on the window
 * or simply made to be on the upper-left corner on the window.  This function
 * lets you configure this property.
 **/
void
ganv_canvas_set_center_scroll_region(GanvCanvas* canvas, gboolean center_scroll_region)
{
	g_return_if_fail(GANV_IS_CANVAS(canvas));

	canvas->center_scroll_region = center_scroll_region != 0;

	scroll_to(canvas,
	          canvas->layout.hadjustment->value,
	          canvas->layout.vadjustment->value);
}

/**
 * ganv_canvas_get_center_scroll_region:
 * @canvas: A canvas.
 *
 * Returns whether the canvas is set to center the scrolling region in the window
 * if the former is smaller than the canvas' allocation.
 *
 * Return value: Whether the scroll region is being centered in the canvas window.
 **/
gboolean
ganv_canvas_get_center_scroll_region(GanvCanvas* canvas)
{
	g_return_val_if_fail(GANV_IS_CANVAS(canvas), FALSE);

	return canvas->center_scroll_region ? TRUE : FALSE;
}

/**
 * ganv_canvas_set_pixels_per_unit:
 * @canvas: A canvas.
 * @n: The number of pixels that correspond to one canvas unit.
 *
 * Sets the zooming factor of a canvas by specifying the number of pixels that
 * correspond to one canvas unit.
 *
 * The anchor point for zooming, i.e. the point that stays fixed and all others
 * zoom inwards or outwards from it, depends on whether the canvas is set to
 * center the scrolling region or not.  You can control this using the
 * ganv_canvas_set_center_scroll_region() function.  If the canvas is set to
 * center the scroll region, then the center of the canvas window is used as the
 * anchor point for zooming.  Otherwise, the upper-left corner of the canvas
 * window is used as the anchor point.
 **/
void
ganv_canvas_set_pixels_per_unit(GanvCanvas* canvas, double n)
{
	double ax, ay;
	int    x1, y1;
	int    anchor_x, anchor_y;

	g_return_if_fail(GANV_IS_CANVAS(canvas));
	g_return_if_fail(n > 1e-10);

	if (canvas->center_scroll_region) {
		anchor_x = GTK_WIDGET(canvas)->allocation.width / 2;
		anchor_y = GTK_WIDGET(canvas)->allocation.height / 2;
	} else {
		anchor_x = anchor_y = 0;
	}

	/* Find the coordinates of the anchor point in units. */
	if (canvas->layout.hadjustment) {
		ax
		    = (canvas->layout.hadjustment->value
		       + anchor_x) / canvas->pixels_per_unit + canvas->scroll_x1 + canvas->zoom_xofs;
	} else {
		ax = (0.0 + anchor_x) / canvas->pixels_per_unit + canvas->scroll_x1 + canvas->zoom_xofs;
	}
	if (canvas->layout.hadjustment) {
		ay
		    = (canvas->layout.vadjustment->value
		       + anchor_y) / canvas->pixels_per_unit + canvas->scroll_y1 + canvas->zoom_yofs;
	} else {
		ay = (0.0 + anchor_y) / canvas->pixels_per_unit + canvas->scroll_y1 + canvas->zoom_yofs;
	}

	/* Now calculate the new offset of the upper left corner. */
	x1 = ((ax - canvas->scroll_x1) * n) - anchor_x;
	y1 = ((ay - canvas->scroll_y1) * n) - anchor_y;

	canvas->pixels_per_unit = n;

	scroll_to(canvas, x1, y1);

	ganv_canvas_request_update(canvas);
	canvas->need_repick = TRUE;
}

/**
 * ganv_canvas_scroll_to:
 * @canvas: A canvas.
 * @cx: Horizontal scrolling offset in canvas pixel units.
 * @cy: Vertical scrolling offset in canvas pixel units.
 *
 * Makes a canvas scroll to the specified offsets, given in canvas pixel units.
 * The canvas will adjust the view so that it is not outside the scrolling
 * region.  This function is typically not used, as it is better to hook
 * scrollbars to the canvas layout's scrolling adjusments.
 **/
void
ganv_canvas_scroll_to(GanvCanvas* canvas, int cx, int cy)
{
	g_return_if_fail(GANV_IS_CANVAS(canvas));

	scroll_to(canvas, cx, cy);
}

/**
 * ganv_canvas_get_scroll_offsets:
 * @canvas: A canvas.
 * @cx: Horizontal scrolling offset (return value).
 * @cy: Vertical scrolling offset (return value).
 *
 * Queries the scrolling offsets of a canvas.  The values are returned in canvas
 * pixel units.
 **/
void
ganv_canvas_get_scroll_offsets(GanvCanvas* canvas, int* cx, int* cy)
{
	g_return_if_fail(GANV_IS_CANVAS(canvas));

	if (cx) {
		*cx = canvas->layout.hadjustment->value;
	}

	if (cy) {
		*cy = canvas->layout.vadjustment->value;
	}
}

/**
 * ganv_canvas_get_item_at:
 * @canvas: A canvas.
 * @x: X position in world coordinates.
 * @y: Y position in world coordinates.
 *
 * Looks for the item that is under the specified position, which must be
 * specified in world coordinates.
 *
 * Return value: (transfer none): The sought item, or NULL if no item is at the
 * specified coordinates.
 **/
GanvItem*
ganv_canvas_get_item_at(GanvCanvas* canvas, double x, double y)
{
	GanvItem* item;
	double    dist;
	int       cx, cy;

	g_return_val_if_fail(GANV_IS_CANVAS(canvas), NULL);

	ganv_canvas_w2c(canvas, x, y, &cx, &cy);

	dist = GANV_ITEM_GET_CLASS(canvas->root)->point(
		canvas->root,
		x - canvas->root->x, y - canvas->root->y,
		cx, cy,
		&item);
	if ((int)(dist * canvas->pixels_per_unit + 0.5) <= canvas->close_enough) {
		return item;
	} else {
		return NULL;
	}
}

/* Queues an update of the canvas */
void
ganv_canvas_request_update(GanvCanvas* canvas)
{
	GANV_CANVAS_GET_CLASS(canvas)->request_update(canvas);
}

static void
ganv_canvas_request_update_real(GanvCanvas* canvas)
{
	if (canvas->need_update) {
		return;
	}

	canvas->need_update = TRUE;
	if (GTK_WIDGET_MAPPED((GtkWidget*)canvas)) {
		add_idle(canvas);
	}
}

static inline gboolean
rect_overlaps(const IRect* a, const IRect* b)
{
	if ((a->x             > b->x + b->width) ||
	    (a->y             > b->y + b->height) ||
	    (a->x + a->width  < b->x) ||
	    (a->y + a->height < b->y)) {
		return FALSE;
	}
	return TRUE;
}

static inline gboolean
rect_is_visible(GanvCanvas* canvas, const IRect* r)
{
	const IRect rect = {
		(int)(canvas->layout.hadjustment->value - canvas->zoom_xofs),
		(int)(canvas->layout.vadjustment->value - canvas->zoom_yofs),
		GTK_WIDGET(canvas)->allocation.width,
		GTK_WIDGET(canvas)->allocation.height
	};

	return rect_overlaps(&rect, r);
}

/**
 * ganv_canvas_request_redraw:
 * @canvas: A canvas.
 * @x1: Leftmost coordinate of the rectangle to be redrawn.
 * @y1: Upper coordinate of the rectangle to be redrawn.
 * @x2: Rightmost coordinate of the rectangle to be redrawn, plus 1.
 * @y2: Lower coordinate of the rectangle to be redrawn, plus 1.
 *
 * Informs a canvas that the specified area needs to be repainted.  To be used
 * only by item implementations.
 **/
void
ganv_canvas_request_redraw(GanvCanvas* canvas, int x1, int y1, int x2, int y2)
{
	g_return_if_fail(GANV_IS_CANVAS(canvas));

	if (!GTK_WIDGET_DRAWABLE(canvas) || (x1 >= x2) || (y1 >= y2)) {
		return;
	}

	const IRect rect = { x1, y1, x2 - x1, y2 - y1 };

	if (!rect_is_visible(canvas, &rect)) {
		return;
	}

	IRect* r = (IRect*)g_malloc(sizeof(IRect));
	*r = rect;

	canvas->redraw_region = g_slist_prepend(canvas->redraw_region, r);
	canvas->need_redraw   = TRUE;

	if (canvas->idle_id == 0) {
		add_idle(canvas);
	}
}

/**
 * ganv_canvas_w2c_affine:
 * @canvas: A canvas.
 * @matrix: An affine transformation matrix (return value).
 *
 * Gets the affine transform that converts from world coordinates to canvas
 * pixel coordinates.
 **/
void
ganv_canvas_w2c_affine(GanvCanvas* canvas, cairo_matrix_t* matrix)
{
	g_return_if_fail(GANV_IS_CANVAS(canvas));
	g_return_if_fail(matrix != NULL);

	cairo_matrix_init_translate(matrix,
	                            -canvas->scroll_x1,
	                            -canvas->scroll_y1);

	cairo_matrix_scale(matrix,
	                   canvas->pixels_per_unit,
	                   canvas->pixels_per_unit);
}

/**
 * ganv_canvas_w2c:
 * @canvas: A canvas.
 * @wx: World X coordinate.
 * @wy: World Y coordinate.
 * @cx: X pixel coordinate (return value).
 * @cy: Y pixel coordinate (return value).
 *
 * Converts world coordinates into canvas pixel coordinates.
 **/
void
ganv_canvas_w2c(GanvCanvas* canvas, double wx, double wy, int* cx, int* cy)
{
	g_return_if_fail(GANV_IS_CANVAS(canvas));

	cairo_matrix_t matrix;
	ganv_canvas_w2c_affine(canvas, &matrix);

	cairo_matrix_transform_point(&matrix, &wx, &wy);

	if (cx) {
		*cx = floor(wx + 0.5);
	}
	if (cy) {
		*cy = floor(wy + 0.5);
	}
}

/**
 * ganv_canvas_w2c_d:
 * @canvas: A canvas.
 * @wx: World X coordinate.
 * @wy: World Y coordinate.
 * @cx: X pixel coordinate (return value).
 * @cy: Y pixel coordinate (return value).
 *
 * Converts world coordinates into canvas pixel coordinates.  This
 * version returns coordinates in floating point coordinates, for
 * greater precision.
 **/
void
ganv_canvas_w2c_d(GanvCanvas* canvas, double wx, double wy, double* cx, double* cy)
{
	g_return_if_fail(GANV_IS_CANVAS(canvas));

	cairo_matrix_t matrix;
	ganv_canvas_w2c_affine(canvas, &matrix);

	cairo_matrix_transform_point(&matrix, &wx, &wy);

	if (cx) {
		*cx = wx;
	}
	if (cy) {
		*cy = wy;
	}
}

/**
 * ganv_canvas_c2w:
 * @canvas: A canvas.
 * @cx: Canvas pixel X coordinate.
 * @cy: Canvas pixel Y coordinate.
 * @wx: X world coordinate (return value).
 * @wy: Y world coordinate (return value).
 *
 * Converts canvas pixel coordinates to world coordinates.
 **/
void
ganv_canvas_c2w(GanvCanvas* canvas, int cx, int cy, double* wx, double* wy)
{
	g_return_if_fail(GANV_IS_CANVAS(canvas));

	cairo_matrix_t matrix;
	ganv_canvas_w2c_affine(canvas, &matrix);
	cairo_matrix_invert(&matrix);

	double x = cx;
	double y = cy;
	cairo_matrix_transform_point(&matrix, &x, &y);

	if (wx) {
		*wx = x;
	}
	if (wy) {
		*wy = y;
	}
}

/**
 * ganv_canvas_window_to_world:
 * @canvas: A canvas.
 * @winx: Window-relative X coordinate.
 * @winy: Window-relative Y coordinate.
 * @worldx: X world coordinate (return value).
 * @worldy: Y world coordinate (return value).
 *
 * Converts window-relative coordinates into world coordinates.  You can use
 * this when you need to convert mouse coordinates into world coordinates, for
 * example.
 **/
void
ganv_canvas_window_to_world(GanvCanvas* canvas, double winx, double winy,
                                 double* worldx, double* worldy)
{
	g_return_if_fail(GANV_IS_CANVAS(canvas));

	if (worldx) {
		*worldx = canvas->scroll_x1 + ((winx - canvas->zoom_xofs)
		                               / canvas->pixels_per_unit);
	}

	if (worldy) {
		*worldy = canvas->scroll_y1 + ((winy - canvas->zoom_yofs)
		                               / canvas->pixels_per_unit);
	}
}

/**
 * ganv_canvas_world_to_window:
 * @canvas: A canvas.
 * @worldx: World X coordinate.
 * @worldy: World Y coordinate.
 * @winx: X window-relative coordinate.
 * @winy: Y window-relative coordinate.
 *
 * Converts world coordinates into window-relative coordinates.
 **/
void
ganv_canvas_world_to_window(GanvCanvas* canvas, double worldx, double worldy,
                                 double* winx, double* winy)
{
	g_return_if_fail(GANV_IS_CANVAS(canvas));

	if (winx) {
		*winx = (canvas->pixels_per_unit) * (worldx - canvas->scroll_x1) + canvas->zoom_xofs;
	}

	if (winy) {
		*winy = (canvas->pixels_per_unit) * (worldy - canvas->scroll_y1) + canvas->zoom_yofs;
	}
}

} // extern "C"
