/* This file is part of Ganv.
 * Copyright 2007-2015 David Robillard <http://drobilla.net>
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

#ifndef GANV_CANVAS_HPP
#define GANV_CANVAS_HPP

#include <string>

#include <glib.h>
#include <glibmm.h>
#include <gtkmm/layout.h>

#include "ganv/canvas.h"
#include "ganv/wrap.hpp"

/** Ganv namespace, everything is defined under this.
 *
 * @ingroup Ganv
 */
namespace Ganv {

class Edge;
class Item;
class Node;
class Port;

/** @defgroup Ganv Ganv
 *
 * A canvas widget for graph-like UIs.
 */

/** The 'master' canvas widget which contains all other objects.
 *
 * Applications must override some virtual methods to make the widget actually
 * do anything (e.g. connect).
 *
 * @ingroup Ganv
 */
class Canvas
{
public:
	Canvas(double width, double height);

	Canvas(const Canvas&) = delete;
	Canvas& operator=(const Canvas&) = delete;

	Canvas(Canvas&&)  = delete;
	Canvas& operator=(Canvas&&) = delete;

	virtual ~Canvas();

	GanvItem* root() { return ganv_canvas_root(gobj()); }

	METHOD0(ganv_canvas, clear)
	METHODRET0(ganv_canvas, gboolean, empty)
	METHOD2(ganv_canvas, get_size, double*, width, double*, height)
	METHOD2(ganv_canvas, resize, double, width, double, height)
	METHOD4(ganv_canvas, set_scroll_region, double, x1, double, y1, double, x2, double, y2)
	METHOD4(ganv_canvas, get_scroll_region, double*, x1, double*, y1, double*, x2, double*, y2)
	METHOD1(ganv_canvas, set_center_scroll_region, gboolean, c)
	METHODRET0(ganv_canvas, gboolean, get_center_scroll_region)
	METHOD2(ganv_canvas, scroll_to, int, x, int, y)

	void get_scroll_offsets(int& cx, int& cy) const {
		ganv_canvas_get_scroll_offsets(gobj(), &cx, &cy);
	}

	METHOD1(ganv_canvas, w2c_affine, cairo_matrix_t*, matrix)
	METHOD4(ganv_canvas, w2c, double, wx, double, wy, int*, cx, int*, cy)
	METHOD4(ganv_canvas, w2c_d, double, wx, double, wy, double*, cx, double*, cy)
	METHOD4(ganv_canvas, c2w, int, cx, int, cy, double*, wx, double*, wy)
	METHOD4(ganv_canvas, window_to_world, double, winx, double, winy, double*, worldx, double*, worldy)
	METHOD4(ganv_canvas, world_to_window, double, worldx, double, worldy, double*, winx, double*, winy)

	Item* get_item_at(double x, double y) const;
	Edge* get_edge(Node* tail, Node* head) const;
	void  remove_edge_between(Node* tail, Node* head);
	void  remove_edge(Edge* edge);

	METHOD0(ganv_canvas, arrange)
	METHODRET2(ganv_canvas, int, export_image, const char*, filename, bool, draw_background)
	METHOD1(ganv_canvas, export_dot, const char*, filename)
	METHODRET0(ganv_canvas, gboolean, supports_sprung_layout)
	METHODRET1(ganv_canvas, gboolean, set_sprung_layout, gboolean, sprung_layout)
	METHOD2(ganv_canvas, for_each_node, GanvNodeFunc, f, void*, data)
	METHOD2(ganv_canvas, for_each_selected_node, GanvNodeFunc, f, void*, data)
	METHOD2(ganv_canvas, for_each_edge, GanvEdgeFunc, f, void*, data)
	METHOD3(ganv_canvas, for_each_edge_from,
	        const GanvNode*, tail,
	        GanvEdgeFunc, f,
	        void*, data)
	METHOD3(ganv_canvas, for_each_edge_to,
	        const GanvNode*, head,
	        GanvEdgeFunc, f,
	        void*, data)
	METHOD3(ganv_canvas, for_each_edge_on,
	        const GanvNode*, node,
	        GanvEdgeFunc, f,
	        void*, data)
	METHOD2(ganv_canvas, for_each_selected_edge, GanvEdgeFunc, f, void*, data)

	METHOD0(ganv_canvas, select_all)
	METHOD0(ganv_canvas, clear_selection)
	METHODRET0(ganv_canvas, double, get_zoom)
	METHOD1(ganv_canvas, set_zoom, double, pix_per_unit)
	METHOD0(ganv_canvas, zoom_full)
	METHODRET0(ganv_canvas, double, get_default_font_size)
	METHODRET0(ganv_canvas, double, get_font_size)
	METHOD1(ganv_canvas, set_font_size, double, points)
	METHOD0(ganv_canvas, get_move_cursor)
	METHOD2(ganv_canvas, move_contents_to, double, x, double, y)

	RW_PROPERTY(gboolean, locked)
	RW_PROPERTY(double, width)
	RW_PROPERTY(double, height)
	RW_PROPERTY(GanvDirection, direction)

	void set_port_order(GanvPortOrderFunc port_cmp, void* data) {
		ganv_canvas_set_port_order(gobj(), port_cmp, data);
	}

	Gtk::Layout& widget() {
		return *Glib::wrap(&_gobj->layout);
	}

	GQuark wrapper_key();

	GanvCanvas*       gobj()       { return GANV_CANVAS(_gobj); }
	const GanvCanvas* gobj() const { return GANV_CANVAS(_gobj); }

	sigc::signal<bool, GdkEvent*>    signal_event;
	sigc::signal<void, Node*, Node*> signal_connect;
	sigc::signal<void, Node*, Node*> signal_disconnect;

private:
	GanvCanvas* const _gobj;
};

} // namespace Ganv

namespace Glib {

static inline Ganv::Canvas*
wrap(GanvCanvas* canvas)
{
	return static_cast<Ganv::Canvas*>(ganv_canvas_get_wrapper(canvas));
}

} // namespace Glib

#endif // GANV_CANVAS_HPP
