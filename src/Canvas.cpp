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

#include <libgnomecanvas/gnome-canvas-pixbuf.h>

#include "ganv-config.h"
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

#ifdef HAVE_AGRAPH
#    include <gvc.h>
#endif

static guint signal_connect;
static guint signal_disconnect;

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

#ifdef HAVE_AGRAPH
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
		return ((a->tail < b->tail)
		        || (a->tail == b->tail && a->head < b->head));
	}
};

/** Order edges by (head, tail) */
struct HeadTailOrder {
	inline bool operator()(const GanvEdge* a, const GanvEdge* b) const {
		return ((a->head < b->head)
		        || (a->head == b->head && a->tail < b->tail));
	}
};

struct GanvCanvasImpl {
	GanvCanvasImpl(GanvCanvas* gobj)
		: _gcanvas(gobj)
		, _layout(GTK_LAYOUT(_gcanvas))
		, _connect_port(NULL)
		, _last_selected_port(NULL)
		, _base_rect(gnome_canvas_item_new(
			             gnome_canvas_root(GNOME_CANVAS(_gcanvas)),
			             gnome_canvas_rect_get_type(),
			             "x1", 0.0,
			             "y1", 0.0,
			             "x2", 1.0,
			             "y2", 1.0,
			             "fill-color-rgba", 0x000000FF,
			             NULL))
		, _select_rect(NULL)
		, _zoom(1.0)
		, _font_size(0.0)
		, _drag_state(NOT_DRAGGING)
		, _direction(Ganv::Canvas::HORIZONTAL)
	{
		_wrapper_key = g_quark_from_string("ganvmm");
		_move_cursor = gdk_cursor_new(GDK_FLEUR);

		g_signal_connect(G_OBJECT(gnome_canvas_root(GNOME_CANVAS(_gcanvas))),
		                 "event", G_CALLBACK(on_canvas_event), this);

	}

	~GanvCanvasImpl()
	{
		gdk_cursor_unref(_move_cursor);
	}

	static gboolean
	on_canvas_event(GnomeCanvasItem* canvasitem,
	                GdkEvent*        ev,
	                void*            impl)
	{
		return ((GanvCanvasImpl*)impl)->on_event(ev);
	}

	GnomeCanvasGroup* root() {
		return gnome_canvas_root(GNOME_CANVAS(_gcanvas));
	}

	void resize(double width, double height);

	void for_each_edge_from(const GanvNode* tail, GanvEdgeFunction f);
	void for_each_edge_to(const GanvNode* head, GanvEdgeFunction f);
	void for_each_edge_on(const GanvNode* node, GanvEdgeFunction f);

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

#ifdef HAVE_AGRAPH
	GVNodes layout_dot(bool use_length_hints, const std::string& filename);
#endif

	void remove_edge(GanvEdge* c);
	bool are_connected(const GanvNode* tail,
	                   const GanvNode* head);

	typedef std::set<GanvEdge*, TailHeadOrder> Edges;
	typedef std::set<GanvEdge*, HeadTailOrder> DstEdges;
	typedef std::set<GanvEdge*>                SelectedEdges;
	typedef std::list<GanvPort*>               SelectedPorts;

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

	/**
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

	SelectedPorts   _selected_ports; ///< Selected ports (hilited red)
	GanvPort* _connect_port; ///< Port for which a edge is being made
	GanvPort* _last_selected_port;

	GnomeCanvasItem* _base_rect;    ///< Background
	GanvBox*   _select_rect;  ///< Rectangle for drag selection

	double _zoom;       ///< Current zoom level
	double _font_size;  ///< Current font size in points

	enum DragState { NOT_DRAGGING, EDGE, SCROLL, SELECT };
	DragState      _drag_state;

	GQuark     _wrapper_key;
	GdkCursor* _move_cursor;

	Ganv::Canvas::FlowDirection _direction;
};

static GanvEdge
make_edge_search_key(const GanvNode* tail, const GanvNode* head)
{
	GanvEdge key;
	memset(&key, '\0', sizeof(GanvEdge));
	key.tail = const_cast<GanvNode*>(tail);
	key.head = const_cast<GanvNode*>(head);
	return key;
}

GanvCanvasImpl::Edges::const_iterator
GanvCanvasImpl::first_edge_from(const GanvNode* tail)
{
	GanvEdge key = make_edge_search_key(tail, NULL);
	return _edges.lower_bound(&key);
}

GanvCanvasImpl::DstEdges::const_iterator
GanvCanvasImpl::first_edge_to(const GanvNode* head)
{
	GanvEdge key = make_edge_search_key(NULL, head);
	return _dst_edges.lower_bound(&key);
}

void
GanvCanvasImpl::select_edge(GanvEdge* edge)
{
	gnome_canvas_item_set(GNOME_CANVAS_ITEM(edge), "selected", TRUE, NULL);
	_selected_edges.insert(edge);
}

void
GanvCanvasImpl::unselect_edge(GanvEdge* edge)
{
	gnome_canvas_item_set(GNOME_CANVAS_ITEM(edge), "selected", FALSE, NULL);
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
		std::cerr << "FIXME: selection move finished" << std::endl;
		//Glib::wrap(*i)->signal_moved.emit();
	}
}

static void
select_if_tail_is_selected(GanvEdge* edge)
{
	gboolean selected;
	g_object_get(edge->tail, "selected", &selected, NULL);
	if (!selected && GANV_IS_PORT(edge->tail)) {
		g_object_get(ganv_port_get_module(GANV_PORT(edge->tail)),
		             "selected", &selected, NULL);
	}
		
	if (selected) {
		ganv_edge_select(edge);
	}
}

static void
select_if_head_is_selected(GanvEdge* edge)
{
	gboolean selected;
	g_object_get(edge->head, "selected", &selected, NULL);
	if (!selected && GANV_IS_PORT(edge->head)) {
		g_object_get(ganv_port_get_module(GANV_PORT(edge->head)),
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
	if (port->is_input) {
		impl->for_each_edge_to(GANV_NODE(port),
		                       select_if_tail_is_selected);
	} else {
		impl->for_each_edge_from(GANV_NODE(port),
		                         select_if_head_is_selected);
	}
}

void
GanvCanvasImpl::add_item(GanvNode* n)
{
	if (GNOME_CANVAS_ITEM(n)->parent == GNOME_CANVAS_ITEM(root())) {
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

	// Remove from selection
	_selected_items.erase(item);

	// Remove children ports from selection if item is a module
	if (GANV_IS_MODULE(item)) {
		GanvModule* const module = GANV_MODULE(item);
		for (unsigned i = 0; i < ganv_module_num_ports(module); ++i) {
			unselect_port((GanvPort*)g_ptr_array_index(
				              module->ports, i));
		}
	}

	// Remove from items
	_items.erase(item);

	return ret;
}

void
GanvCanvasImpl::select_item(GanvNode* m)
{
	_selected_items.insert(m);

	// Select any connections to or from this node
	if (GANV_IS_MODULE(m)) {
		ganv_module_for_each_port(GANV_MODULE(m), select_edges, this);
	} else {
		for_each_edge_on(m, ganv_edge_select);
	}

	g_object_set(m, "selected", TRUE, NULL);
}

static void
unselect_edges(GanvPort* port, void* data)
{
	GanvCanvasImpl* impl = (GanvCanvasImpl*)data;
	if (port->is_input) {
		impl->for_each_edge_to(GANV_NODE(port),
		                       ganv_edge_unselect);
	} else {
		impl->for_each_edge_from(GANV_NODE(port),
		                         ganv_edge_unselect);
	}
}

void
GanvCanvasImpl::unselect_item(GanvNode* m)
{
	// Unselect any connections to or from this node
	if (GANV_IS_MODULE(m)) {
		ganv_module_for_each_port(GANV_MODULE(m), unselect_edges, this);
	} else {
		for_each_edge_on(m, ganv_edge_unselect);
	}

	// Unselect item
	_selected_items.erase(m);
	g_object_set(m, "selected", FALSE, NULL);
}

#ifdef HAVE_AGRAPH
static void
gv_set(void* subject, const char* key, double value)
{
	std::ostringstream ss;
	ss << value;
	agsafeset(subject, (char*)key, (char*)ss.str().c_str(), NULL);
}

GVNodes
GanvCanvasImpl::layout_dot(bool use_length_hints, const std::string& filename)
{
	GVNodes nodes;

	const double dpi = gdk_screen_get_resolution(gdk_screen_get_default());

	GVC_t*    gvc = gvContext();
	Agraph_t* G   = agopen((char*)"g", AGDIGRAPH);
	agsafeset(G, (char*)"splines", (char*)"false", NULL);
	agsafeset(G, (char*)"compound", (char*)"true", NULL);
	agsafeset(G, (char*)"remincross", (char*)"true", NULL);
	agsafeset(G, (char*)"overlap", (char*)"scale", NULL);
	agsafeset(G, (char*)"nodesep", (char*)"0.0", NULL);
	gv_set(G, "fontsize", ganv_canvas_get_font_size(_gcanvas));
	gv_set(G, "dpi", dpi);

	nodes.gvc = gvc;
	nodes.G   = G;

	if (_direction == Ganv::Canvas::HORIZONTAL) {
		agraphattr(G, (char*)"rankdir", (char*)"LR");
	} else {
		agraphattr(G, (char*)"rankdir", (char*)"TD");
	}

	unsigned id = 0;
	std::ostringstream ss;
	FOREACH_ITEM(_items, i) {
		if (GANV_IS_MODULE(*i)) {
			// Create a subgraph for this node
			ss.str("");
			ss << "cluster" << id++;
			Agraph_t* subg = agsubg(G, (char*)ss.str().c_str());
			agsafeset(subg, (char*)"shape", (char*)"box", NULL);
			agsafeset(subg, (char*)"pad", (char*)"0", NULL);
			gv_set(subg, "width",
			       ganv_box_get_width(GANV_BOX(*i)) / dpi);
			gv_set(subg, "height",
			       ganv_box_get_height(GANV_BOX(*i)) / dpi);

			// Make a borderless node for the title (instead of simply setting
			// a title on the subgraph, so partners can be connected)
			ss.str("");
			ss << "n" << id++;
			Agnode_t* node = agnode(subg, strdup(ss.str().c_str()));
			agsafeset(node, (char*)"pos", (char*)"0,0", NULL);
			agsafeset(node, (char*)"shape", (char*)"box", NULL);
			agsafeset(node, (char*)"penwidth", (char*)"0", NULL);
			agsafeset(node, (char*)"margin", (char*)"0", NULL);
			agsafeset(node, (char*)"pad", (char*)"0", NULL);
			const char* label = ganv_node_get_label(GANV_NODE(*i));
			if (label) {
				agsafeset(node, (char*)"label", (char*)label, NULL);
				double width, height;
				g_object_get((*i)->label, "width", &width, "height", &height, NULL);
				gv_set(node, "width", width / dpi);
				gv_set(node, "height", height / dpi);
			}
			nodes.insert(std::make_pair(*i, (Agnode_t*)node));

			// Make a node in the subgraph for each port on this module
			GanvModule* const m = GANV_MODULE(*i);
			for (size_t i = 0; i < m->ports->len; ++i) {
				GanvPort* port = (GanvPort*)g_ptr_array_index(m->ports, i);
				ss.str("");
				ss << "p" << id++;
				Agnode_t* pnode = agnode(subg, (char*)ss.str().c_str());
				agsafeset(pnode, (char*)"shape", (char*)"box", NULL);
				agsafeset(pnode, (char*)"margin", (char*)"0", NULL);
				agsafeset(pnode, (char*)"pin", (char*)"true", NULL);
				agsafeset(pnode, (char*)"label",
				          (char*)ganv_node_get_label(GANV_NODE(port)),
				          NULL);

				// Fix position (we don't want ports to be arranged)
				// (TODO: I don't think dot actually supports this...)
				double x, y;
				g_object_get(G_OBJECT(port), "x", &x, "y", &y, NULL);
				ss.str("");
				ss << (x / dpi) << "," << (-y / dpi);
				agsafeset(pnode, (char*)"pos", (char*)ss.str().c_str(), NULL);
				gv_set(pnode, "width",
				       ganv_box_get_width(GANV_BOX(port)) / dpi);
				gv_set(pnode, "height",
				       ganv_box_get_height(GANV_BOX(port)) / dpi);
				nodes.insert(std::make_pair(GANV_NODE(port), pnode));
			}
		} else if (GANV_IS_CIRCLE(*i)) {
			ss.str("");
			ss << "n" << id++;
			Agnode_t* node = agnode(G, strdup(ss.str().c_str()));
			agsafeset(node, (char*)"width", (char*)"1.0", NULL);
			agsafeset(node, (char*)"height", (char*)"1.0", NULL);
			agsafeset(node, (char*)"shape", (char*)"ellipse", NULL);
			agsafeset(node, (char*)"label",
			          (char*)ganv_node_get_label(GANV_NODE(*i)),
			          NULL);
			nodes.insert(std::make_pair(*i, node));
		} else {
			std::cerr << "Unable to arrange item of unknown type" << std::endl;
		}
	}

	FOREACH_EDGE(_edges, i) {
		const GanvEdge* const edge = *i;

		GVNodes::iterator tail_i = nodes.find(edge->tail);
		GVNodes::iterator head_i = nodes.find(edge->head);

		if (tail_i != nodes.end() && head_i != nodes.end()) {
			agedge(G, tail_i->second, head_i->second);
		} else {
			std::cerr << "Unable to find graphviz node" << std::endl;
		}
	}

	// Add edges between partners to have them lined up as if they are
	// connected
	for (GVNodes::iterator i = nodes.begin(); i != nodes.end(); ++i) {
		GanvNode* partner = ganv_node_get_partner(i->first);
		if (partner) {
			GVNodes::iterator p = nodes.find(partner);
			if (p != nodes.end()) {
				Agedge_t* e = agedge(G, i->second, p->second);
				agsafeset(e, (char*)"style", (char*)"dotted", NULL);
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

void
GanvCanvasImpl::remove_edge(GanvEdge* edge)
{
	if (edge) {
		_selected_edges.erase(edge);
		_edges.erase(edge);
		_dst_edges.erase(edge);
		gtk_object_destroy(GTK_OBJECT(edge));
	}
}

/** Return whether there is a edge between item1 and item2.
 *
 * Note that edges are directed, so this may return false when there
 * is a edge between the two items (in the opposite direction).
 */
bool
GanvCanvasImpl::are_connected(const GanvNode* tail,
                              const GanvNode* head)
{
	GanvEdge key = make_edge_search_key(tail, head);
	return (_edges.find(&key) != _edges.end());
}

void
GanvCanvasImpl::select_port(GanvPort* p, bool unique)
{
	if (unique) {
		unselect_ports();
	}
	g_object_set(G_OBJECT(p), "selected", TRUE, NULL);
	SelectedPorts::iterator i = find(_selected_ports.begin(), _selected_ports.end(), p);
	if (i == _selected_ports.end())
		_selected_ports.push_back(p);
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
			bool            done              = false;
			for (size_t i = 0; i < m->ports->len; ++i) {
				GanvPort* const p = (GanvPort*)g_ptr_array_index(m->ports, i);
				if (!first && !done && (p == _last_selected_port || p == port)) {
					first = p;
				}

				if (first && !done && p->is_input == first->is_input) {
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
	SelectedPorts::iterator i = find(_selected_ports.begin(), _selected_ports.end(), p);
	if (i != _selected_ports.end())
		_selected_ports.erase(i);
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
		if ((*i)->is_input) {
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
	GnomeCanvasItem* item = gnome_canvas_get_item_at(GNOME_CANVAS(_gcanvas), x, y);
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
	gnome_canvas_get_scroll_offsets(GNOME_CANVAS(_gcanvas), &scroll_x, &scroll_y);

	switch (event->type) {
	case GDK_KEY_PRESS:
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
			return true;
		default:
			return false;
		}
		gnome_canvas_scroll_to(GNOME_CANVAS(_gcanvas), scroll_x, scroll_y);
		return true;

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
		gnome_canvas_item_grab(
			GNOME_CANVAS_ITEM(_base_rect),
			GDK_POINTER_MOTION_MASK|GDK_BUTTON_RELEASE_MASK,
			NULL, event->button.time);
		gnome_canvas_get_scroll_offsets(GNOME_CANVAS(_gcanvas), &original_scroll_x, &original_scroll_y);
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
		gnome_canvas_scroll_to(GNOME_CANVAS(_gcanvas),
		                       lrint(original_scroll_x + scroll_offset_x),
		                       lrint(original_scroll_y + scroll_offset_y));
		last_x = x;
		last_y = y;
	} else if (event->type == GDK_BUTTON_RELEASE && _drag_state == SCROLL) {
		gnome_canvas_item_ungrab(GNOME_CANVAS_ITEM(_base_rect), event->button.time);
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
			gnome_canvas_item_new(
				root(),
				ganv_box_get_type(),
				"x1", event->button.x,
				"y1", event->button.y,
				"x2", event->button.x,
				"y2", event->button.y,
				"fill-color", SELECT_RECT_FILL_COLOUR,
				"border-color", SELECT_RECT_BORDER_COLOUR,
				NULL));
		gnome_canvas_item_lower_to_bottom(GNOME_CANVAS_ITEM(_select_rect));
		gnome_canvas_item_lower_to_bottom(GNOME_CANVAS_ITEM(_base_rect));
		gnome_canvas_item_grab(
			GNOME_CANVAS_ITEM(_base_rect), GDK_POINTER_MOTION_MASK|GDK_BUTTON_RELEASE_MASK,
			NULL, event->button.time);
		return true;
	} else if (event->type == GDK_MOTION_NOTIFY && _drag_state == SELECT) {
		assert(_select_rect);
		double x, y;
		get_motion_coords(&event->motion, &x, &y);
		gnome_canvas_item_set(GNOME_CANVAS_ITEM(_select_rect),
		                      "x2", x,
		                      "y2", y,
		                      NULL);
		return true;
	} else if (event->type == GDK_BUTTON_RELEASE && _drag_state == SELECT) {
		// Normalize select rect
		ganv_box_normalize(_select_rect);

		// Select all modules within rect
		FOREACH_ITEM(_items, i) {
			GanvNode* node = *i;
			if (ganv_node_is_within(
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

		gnome_canvas_item_ungrab(GNOME_CANVAS_ITEM(_base_rect), event->button.time);

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
	static GanvEdge* drag_edge = NULL;
	static GanvNode* drag_node = NULL;
	static bool            snapped   = false;

	if (_drag_state != EDGE) {
		return false;
	}

	if (event->type == GDK_MOTION_NOTIFY) {
		double x, y;
		get_motion_coords(&event->motion, &x, &y);
		gnome_canvas_item_i2w(GNOME_CANVAS_ITEM(_base_rect), &x, &y);

		if (!drag_edge) {
			// Create drag edge
			assert(!drag_node);
			assert(_connect_port);

			drag_node = GANV_NODE(
				gnome_canvas_item_new(gnome_canvas_root(GNOME_CANVAS(_gcanvas)),
				                      ganv_node_get_type(),
				                      "x", x,
				                      "y", y,
				                      NULL));

			drag_edge = ganv_edge_new(
				_gcanvas,
				GANV_NODE(_connect_port),
				drag_node,
				"color", GANV_NODE(_connect_port)->fill_color,
				"curved", TRUE,
				"ghost", TRUE,
				NULL);
		}

		GanvNode* joinee = get_node_at(x, y);
		if (joinee && ganv_node_can_head(joinee) && joinee != drag_node) {
			// Snap to item
			snapped = true;
			gnome_canvas_item_set(&drag_edge->item,
			                      "head", joinee,
			                      NULL);
		} else if (snapped) {
			// Unsnap from item
			snapped = false;
			gnome_canvas_item_set(&drag_edge->item,
			                      "head", drag_node,
			                      NULL);
		}

		// Update drag edge for pointer position
		ganv_node_move_to(drag_node, x, y);
		gnome_canvas_item_request_update(&drag_node->group.item);
		gnome_canvas_item_request_update(&drag_edge->item);

		return true;

	} else if (event->type == GDK_BUTTON_RELEASE) {
		gnome_canvas_item_ungrab(GNOME_CANVAS_ITEM(_base_rect), event->button.time);

		double x = event->button.x;
		double y = event->button.y;
		gnome_canvas_item_i2w(GNOME_CANVAS_ITEM(_base_rect), &x, &y);

		GanvNode* joinee = get_node_at(x, y);

		if (joinee) {
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

		// Clean up dragging stuff

		if (_connect_port) {
			g_object_set(G_OBJECT(_connect_port), "highlighted", FALSE, NULL);
		}

		unselect_ports();
		_drag_state   = NOT_DRAGGING;
		_connect_port = NULL;

		gtk_object_destroy(GTK_OBJECT(drag_edge));
		gtk_object_destroy(GTK_OBJECT(drag_node));
		drag_edge = NULL;
		drag_node = NULL;

		return true;
	}

	return false;
}

bool
GanvCanvasImpl::port_event(GdkEvent* event, GanvPort* port)
{
	static bool port_dragging         = false;
	static bool control_dragging      = false;
	static bool ignore_button_release = false;

	bool handled = true;

	switch (event->type) {

	case GDK_BUTTON_PRESS:
		if (event->button.button == 1) {
			GanvModule* const module = ganv_port_get_module(port);
			if (module && _gcanvas->locked && port->is_input) {
				std::cerr << "FIXME: port control drag" << std::endl;
#if 0
				if (port->is_toggled()) {
					port->toggle();
					ignore_button_release = true;
				} else {
					control_dragging = true;
					const double port_x = module->get_x() + port->get_x();
					float new_control = ((event->button.x - port_x) / (double)port->get_width());
					if (new_control < 0.0)
						new_control = 0.0;
					else if (new_control > 1.0)
						new_control = 1.0;

					new_control *= (port->control_max() - port->control_min());
					new_control += port->control_min();
					if (new_control < port->control_min())
						new_control = port->control_min();
					if (new_control > port->control_max())
						new_control = port->control_max();
					if (new_control != port->control_value())
						port->set_control(new_control);
				}
#endif
			} else {
				port_dragging = true;
			}
		} else {
			// FIXME
			//return Glib::wrap(port)->on_click(&event->button);
		}
		break;

	case GDK_MOTION_NOTIFY:
		if (control_dragging) {
			GanvModule* const module = ganv_port_get_module(port);
			if (module) {
				const double port_x = ganv_box_get_x1(GANV_BOX(module))
					+ ganv_box_get_x1(GANV_BOX(port));
				float new_control = ((event->button.x - port_x)
				                     / ganv_box_get_width(GANV_BOX(port)));
				if (new_control < 0.0)
					new_control = 0.0;
				else if (new_control > 1.0)
					new_control = 1.0;

				new_control *= (port->control->max - port->control->min);
				new_control += port->control->min;
				assert(new_control >= port->control->min);
				assert(new_control <= port->control->max);

				if (new_control != port->control->value) {
					ganv_port_set_control_value(port, new_control);
				}
			}
		}
		break;

	case GDK_BUTTON_RELEASE:
		if (port_dragging) {
			if (_connect_port) { // dragging
				ports_joined(port, _connect_port);
				unselect_ports();
			} else {
				bool modded = event->button.state & (GDK_SHIFT_MASK|GDK_CONTROL_MASK);
				if (!modded && _last_selected_port && _last_selected_port->is_input != port->is_input) {
					selection_joined_with(port);
					unselect_ports();
				} else {
					select_port_toggle(port, event->button.state);
				}
			}
			port_dragging = false;
		} else if (control_dragging) {
			control_dragging = false;
		} else if (ignore_button_release) {
			ignore_button_release = false;
		} else {
			handled = false;
		}
		break;

	case GDK_ENTER_NOTIFY:
		//signal_item_entered.emit(port);
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
			gnome_canvas_item_grab(
				GNOME_CANVAS_ITEM(_base_rect),
				GDK_BUTTON_PRESS_MASK|GDK_POINTER_MOTION_MASK|GDK_BUTTON_RELEASE_MASK,
				NULL, event->crossing.time);
		} else if (!control_dragging) {
			g_object_set(G_OBJECT(port), "highlighted", FALSE, NULL);
		}
		//signal_item_left.emit(port);
		break;

	default:
		handled = false;
	}

	return handled;
}

/** Called when two ports are 'toggled' (connected or disconnected)
 */
void
GanvCanvasImpl::ports_joined(GanvPort* port1, GanvPort* port2)
{
	if (port1 == port2 || !port1 || !port2) {
		return;
	}

	g_object_set(G_OBJECT(port1), "highlighted", FALSE, NULL);
	g_object_set(G_OBJECT(port2), "highlighted", FALSE, NULL);

	GanvNode* src_node;
	GanvNode* dst_node;

	if (port2->is_input && !port1->is_input) {
		src_node = GANV_NODE(port1);
		dst_node = GANV_NODE(port2);
	} else if (!port2->is_input && port1->is_input) {
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
GanvCanvasImpl::for_each_edge_from(const GanvNode*  tail,
                                   GanvEdgeFunction f)
{
	for (GanvCanvasImpl::Edges::const_iterator i = first_edge_from(tail);
	     i != _edges.end() && (*i)->tail == tail;) {
		GanvCanvasImpl::Edges::const_iterator next = i;
		++next;
		f((*i));
		i = next;
	}
}

void
GanvCanvasImpl::for_each_edge_to(const GanvNode*  head,
                                 GanvEdgeFunction f)
{
	for (GanvCanvasImpl::Edges::const_iterator i = first_edge_to(head);
	     i != _dst_edges.end() && (*i)->head == head;) {
		GanvCanvasImpl::Edges::const_iterator next = i;
		++next;
		f((*i));
		i = next;
	}
}

void
GanvCanvasImpl::for_each_edge_on(const GanvNode*  node,
                                 GanvEdgeFunction f)
{
	for_each_edge_from(node, f);
	for_each_edge_to(node, f);
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

	FOREACH_ITEM(_selected_items, i) {
		gnome_canvas_item_set(GNOME_CANVAS_ITEM(*i), "selected", FALSE, NULL);
	}

	FOREACH_SELECTED_EDGE(_selected_edges, c) {
		gnome_canvas_item_set(GNOME_CANVAS_ITEM(*c), "selected", FALSE, NULL);
	}

	_selected_items.clear();
	_selected_edges.clear();
}

void
GanvCanvasImpl::resize(double width, double height)
{
	if (width != _gcanvas->width || height != _gcanvas->height) {
		gnome_canvas_item_set(
			GNOME_CANVAS_ITEM(_base_rect),
			"x2", width,
			"y2", height,
			NULL);
		_gcanvas->width  = width;
		_gcanvas->height = height;
		gnome_canvas_set_scroll_region(GNOME_CANVAS(_gcanvas),
		                               0.0, 0.0, width, height);
	}
}

namespace Ganv {

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

	g_signal_connect(gobj(), "connect",
	                 G_CALLBACK(on_connect), this);
	g_signal_connect(gobj(), "disconnect",
	                 G_CALLBACK(on_disconnect), this);

	Glib::signal_timeout().connect(
		sigc::mem_fun(impl(), &GanvCanvasImpl::animate_selected), 120);
}

Canvas::~Canvas()
{
	destroy();
	delete impl();
}

void
Canvas::set_zoom(double pix_per_unit)
{
	set_zoom_and_font_size(pix_per_unit, impl()->_font_size);
}

void
Canvas::set_font_size(double points)
{
	set_zoom_and_font_size(impl()->_zoom, points);
}

void
Canvas::set_zoom_and_font_size(double zoom, double points)
{
	points = std::max(points, 1.0);
	zoom   = std::max(zoom, 0.01);

	if (zoom == impl()->_zoom && points == impl()->_font_size)
		return;

	if (zoom != impl()->_zoom) {
		impl()->_zoom = zoom;
		gnome_canvas_set_pixels_per_unit(GNOME_CANVAS(impl()->_gcanvas), zoom);
	}

	impl()->_font_size = points;

#if 0
	FOREACH_ITEM(impl()->_items, i) {
		(*i)->redraw_text();
	}
#endif
}

void
Canvas::zoom_full()
{
#if 0
	if (impl()->_items.empty())
		return;

	int win_width, win_height;
	Glib::RefPtr<Gdk::Window> win = GNOME_CANVAS(impl()->_gcanvas)->get_window();
	win->get_size(win_width, win_height);

	// Box containing all canvas items
	double left   = DBL_MAX;
	double right  = DBL_MIN;
	double top    = DBL_MIN;
	double bottom = DBL_MAX;

	FOREACH_ITEM(impl()->_items, i) {
		if ((*i)->get_x() < left)
			left = (*i)->get_x();
		if ((*i)->get_x() + (*i)->width() > right)
			right = (*i)->get_x() + (*i)->width();
		if ((*i)->get_y() < bottom)
			bottom = (*i)->get_y();
		if ((*i)->get_y() + (*i)->height() > top)
			top = (*i)->get_y() + (*i)->height();
	}

	static const double pad = 8.0;

	const double new_zoom = std::min(
		((double)win_width / (double)(right - left + pad*2.0)),
		((double)win_height / (double)(top - bottom + pad*2.0)));

	set_zoom(new_zoom);

	int scroll_x, scroll_y;
	GNOME_CANVAS(impl()->_gcanvas)->w2c(lrintf(left - pad), lrintf(bottom - pad), scroll_x, scroll_y);

	GNOME_CANVAS(impl()->_gcanvas)->scroll_to(scroll_x, scroll_y);
#endif
}

void
Canvas::clear_selection()
{
	impl()->clear_selection();
}

void
Canvas::select_edge(Edge* edge)
{
	impl()->select_edge(edge->gobj());
}

void
Canvas::unselect_edge(Edge* edge)
{
	impl()->unselect_edge(edge->gobj());
}

double
Canvas::get_zoom()
{
	return impl()->_zoom;
}

void
Canvas::set_direction(FlowDirection d)
{
	impl()->_direction = d;
	std::cerr << "FIXME: set direction" << std::endl;
#if 0
	FOREACH_ITEM(impl()->_items, i) {
		Module* mod = dynamic_cast<Module*>(*i);
		if (mod) {
			mod->set_show_port_labels(d == Canvas::HORIZONTAL);
		}
	}
#endif
}

Canvas::FlowDirection
Canvas::direction() const
{
	return impl()->_direction;
}

void
Canvas::select_all()
{
	clear_selection();
	FOREACH_ITEM(impl()->_items, m) {
		impl()->select_item(*m);
	}
}

void
Canvas::select_item(Ganv::Node* m)
{
	impl()->select_item(m->gobj());
}

void
Canvas::unselect_item(Ganv::Node* m)
{
	impl()->unselect_item(m->gobj());
}

void
Canvas::destroy()
{
	impl()->_selected_items.clear();
	impl()->_selected_edges.clear();

	Items items = impl()->_items; // copy
	FOREACH_ITEM(items, i) {
		gtk_object_destroy(GTK_OBJECT(*i));
	}

	impl()->_edges.clear();

	impl()->_selected_ports.clear();
	impl()->_connect_port = NULL;

	impl()->_items.clear();
}

void
Canvas::unselect_ports()
{
	impl()->unselect_ports();
}

void
Canvas::set_default_placement(Node* i)
{
	// Simple cascade.  This will get more clever in the future.
	double x = ((get_width() / 2.0) + (impl()->_items.size() * 25));
	double y = ((get_height() / 2.0) + (impl()->_items.size() * 25));

	i->move_to(x, y);
}

void
Canvas::remove_edge(Node* item1,
                    Node* item2)
{
	Edge* edge = get_edge(item1, item2);
	if (edge) {
		impl()->remove_edge(edge->gobj());
	}
}

/** Get the edge between two items.
 *
 * Note that edges are directed.
 * This will only return a edge from @a tail to @a head.
 */
Edge*
Canvas::get_edge(Node* tail, Node* head) const
{
	FOREACH_EDGE(impl()->_edges, i) {
		const GanvNode* const t = (*i)->tail;
		const GanvNode* const h = (*i)->head;

		if (t == tail->gobj() && h == head->gobj())
			return Glib::wrap(*i);
	}

	return NULL;
}

void
Canvas::render_to_dot(const string& dot_output_filename)
{
#ifdef HAVE_AGRAPH
	GVNodes nodes = impl()->layout_dot(false, dot_output_filename);
	nodes.cleanup();
#endif
}

void
Canvas::arrange(bool use_length_hints)
{
#ifdef HAVE_AGRAPH
	GVNodes nodes = impl()->layout_dot(use_length_hints, "");

	double least_x=HUGE_VAL, least_y=HUGE_VAL, most_x=0, most_y=0;

	// Set numeric locale to POSIX for reading graphviz output with strtod
	char* locale = strdup(setlocale(LC_NUMERIC, NULL));
	setlocale(LC_NUMERIC, "POSIX");

	// Arrange to graphviz coordinates
	for (GVNodes::iterator i = nodes.begin(); i != nodes.end(); ++i) {
		if (GNOME_CANVAS_ITEM(i->first)->parent != GNOME_CANVAS_ITEM(root())) {
			continue;
		}
		const string pos   = agget(i->second, (char*)"pos");
		const string x_str = pos.substr(0, pos.find(","));
		const string y_str = pos.substr(pos.find(",") + 1);
		const double cx    = strtod(x_str.c_str(), NULL) * 1.1;
		const double cy    = strtod(y_str.c_str(), NULL) * 1.1;
		const double w     = ganv_box_get_width(GANV_BOX(i->first));

		/* Dot node positions are supposedly node centers, but things only
		   match up if x is interpreted as center and y as top...
		*/
		const double x = cx - (w / 2.0);
		const double y = -cy;

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

	const double old_width  = get_width();
	const double old_height = get_height();
	const double new_width  = std::max(graph_width + 10.0, old_width);
	const double new_height = std::max(graph_height + 10.0, old_height);
	if (new_width != old_width || new_height != old_height) {
		resize(new_width, new_height);
	}
	nodes.cleanup();

	static const double border_width = impl()->_font_size;
	impl()->move_contents_to_internal(border_width, border_width, least_x, least_y);
	gnome_canvas_scroll_to(GNOME_CANVAS(impl()->_gcanvas), 0, 0);

	FOREACH_ITEM(impl()->_items, i) {
		std::cerr << "FIXME: arrange moved" << std::endl;
		Glib::wrap((*i))->signal_moved.emit();
	}
#endif
}

void
Canvas::move_contents_to(double x, double y)
{
	double min_x=HUGE_VAL, min_y=HUGE_VAL;
	FOREACH_ITEM(impl()->_items, i) {
		double x, y;
		g_object_get(*i, "x", &x, "y", &y, NULL);
		min_x = std::min(min_x, x);
		min_y = std::min(min_y, y);
	}
	impl()->move_contents_to_internal(x, y, min_x, min_y);
}

void
Canvas::resize(double width, double height)
{
	impl()->resize(width, height);
}

void
Canvas::for_each_node(NodeFunction f, void* data)
{
	FOREACH_ITEM(impl()->_items, i) {
		f(*i, data);
	}
}

void
Canvas::for_each_selected_node(NodeFunction f, void* data)
{
	FOREACH_ITEM(impl()->_selected_items, i) {
		f(*i, data);
	}
}

void
Canvas::for_each_edge(EdgePtrFunction f, void* data)
{
	FOREACH_EDGE(impl()->_edges, i) {
		f((*i), data);
	}
}

void
Canvas::for_each_selected_edge(EdgePtrFunction f, void* data)
{
	FOREACH_EDGE(impl()->_selected_edges, i) {
		f((*i), data);
	}
}

GnomeCanvasGroup*
Canvas::root()
{
	return gnome_canvas_root(GNOME_CANVAS(impl()->_gcanvas));
}

Gtk::Layout&
Canvas::widget()
{
	return *Glib::wrap(impl()->_layout);
}

void
Canvas::get_scroll_offsets(int& cx, int& cy) const
{
	gnome_canvas_get_scroll_offsets(GNOME_CANVAS(impl()->_gcanvas), &cx, &cy);
}

void
Canvas::scroll_to(int x, int y)
{
	gnome_canvas_scroll_to(GNOME_CANVAS(impl()->_gcanvas), x, y);
}

GQuark
Canvas::wrapper_key()
{
	return impl()->_wrapper_key;
}

GdkCursor*
Canvas::move_cursor()
{
	return impl()->_move_cursor;
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

#include <libgnomecanvas/libgnomecanvas.h>

#include "ganv/canvas.h"

#include "./boilerplate.h"
#include "./color.h"
#include "./gettext.h"

G_DEFINE_TYPE(GanvCanvas, ganv_canvas, GNOME_TYPE_CANVAS)

static GnomeCanvasClass* parent_class;

enum {
	PROP_0,
	PROP_WIDTH,
	PROP_HEIGHT,
	PROP_LOCKED
};

static void
ganv_canvas_init(GanvCanvas* canvas)
{
	canvas->direction = GANV_HORIZONTAL;
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
	case PROP_LOCKED: {
		const gboolean tmp = g_value_get_boolean(value);
		if (canvas->locked != tmp) {
			GnomeCanvasItem* base = GNOME_CANVAS_ITEM(canvas->impl->_base_rect);
			gnome_canvas_item_set(base,
			                      "fill-color", tmp ? 0x131415FF : 0x000000FF,
			                      NULL);
			canvas->locked = tmp;
			gnome_canvas_item_request_update(base);
		}
		break;
	}
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

	parent_class = GNOME_CANVAS_CLASS(g_type_class_peek_parent(klass));

	gobject_class->set_property = ganv_canvas_set_property;
	gobject_class->get_property = ganv_canvas_get_property;

	g_object_class_install_property(
		gobject_class, PROP_WIDTH, g_param_spec_double(
			"width",
			_("width"),
			_("width of the canvas"),
			0.0, G_MAXDOUBLE,
			800.0,
			(GParamFlags)G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_HEIGHT, g_param_spec_double(
			"height",
			_("height"),
			_("height of the canvas"),
			0.0, G_MAXDOUBLE,
			600.0,
			(GParamFlags)G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_LOCKED, g_param_spec_boolean(
			"locked",
			_("locked"),
			_("whether canvas items are movable"),
			FALSE,
			(GParamFlags)G_PARAM_READWRITE));

	signal_connect = g_signal_new("connect",
	                              ganv_canvas_get_type(),
	                              G_SIGNAL_RUN_FIRST,
	                              0, NULL, NULL, NULL,
	                              G_TYPE_NONE,
	                              2,
	                              ganv_node_get_type(),
	                              ganv_node_get_type(),
	                              0);

	signal_disconnect = g_signal_new("disconnect",
	                                 ganv_canvas_get_type(),
	                                 G_SIGNAL_RUN_FIRST,
	                                 0, NULL, NULL, NULL,
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
	gnome_canvas_set_scroll_region(GNOME_CANVAS(canvas),
	                               0.0, 0.0, width, height);
	return canvas;
}

void
ganv_canvas_resize(GanvCanvas* canvas, double width, double height)
{
	canvas->impl->resize(width, height);
}

GnomeCanvasGroup*
ganv_canvas_get_root(const GanvCanvas* canvas)
{
	return gnome_canvas_root(GNOME_CANVAS(canvas->impl->_gcanvas));
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
	return ganv_canvas_get_default_font_size(canvas);
}

void
ganv_canvas_set_font_size(GanvCanvas* canvas, double points)
{
	g_object_set(canvas, "font-size", points, NULL);
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
ganv_canvas_for_each_edge_from(GanvCanvas*      canvas,
                               const GanvNode*  tail,
                               GanvEdgeFunction f)
{
	canvas->impl->for_each_edge_from(tail, f);
}

void
ganv_canvas_for_each_edge_to(GanvCanvas*      canvas,
                             const GanvNode*  head,
                             GanvEdgeFunction f)
{
	canvas->impl->for_each_edge_to(head, f);
}

void
ganv_canvas_for_each_edge_on(GanvCanvas*      canvas,
                             const GanvNode*  node,
                             GanvEdgeFunction f)
{
	canvas->impl->for_each_edge_on(node, f);
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

} // extern "C"
