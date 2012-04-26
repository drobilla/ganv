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

#ifndef GANV_CANVAS_HPP
#define GANV_CANVAS_HPP

#include <string>

#include <glib.h>
#include <glibmm.h>
#include <gtkmm/layout.h>

#include "ganv/canvas.h"
#include "ganv/wrap.hpp"

GANV_GLIB_WRAP(Canvas)

/** Ganv namespace, everything is defined under this.
 *
 * @ingroup Ganv
 */
namespace Ganv {

class Edge;
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
	virtual	~Canvas();

	METHOD0(ganv_canvas, destroy);
	METHOD0(ganv_canvas, clear_selection);
	METHOD0(ganv_canvas, select_all);
	METHOD0(ganv_canvas, get_zoom);
	METHOD1(ganv_canvas, set_zoom, double, pix_per_unit);
	METHOD1(ganv_canvas, set_font_size, double, points);
	METHOD2(ganv_canvas, set_scale, double, zoom, double, points);
	METHOD0(ganv_canvas, zoom_full);
	METHODRET0(ganv_canvas, double, get_font_size)
	METHODRET0(ganv_canvas, double, get_default_font_size)
	METHOD1(ganv_canvas, export_dot, const char*, filename);
	METHOD0(ganv_canvas, arrange);
	METHOD2(ganv_canvas, move_contents_to, double, x, double, y);
	METHOD2(ganv_canvas, resize, double, width, double, height);
	METHOD2(ganv_canvas, for_each_node, GanvNodeFunc, f, void*, data)
	METHOD2(ganv_canvas, for_each_selected_node, GanvNodeFunc, f, void*, data)

	METHOD2(ganv_canvas, for_each_edge_from,
	        const GanvNode*, tail,
	        GanvEdgeFunc, f);

	METHOD2(ganv_canvas, for_each_edge_to,
	        const GanvNode*, head,
	        GanvEdgeFunc, f);

	METHOD2(ganv_canvas, for_each_edge_on,
	        const GanvNode*, node,
	        GanvEdgeFunc, f);

	METHOD0(ganv_canvas, get_move_cursor);

	RW_PROPERTY(gboolean, locked);
	RW_PROPERTY(double, width)
	RW_PROPERTY(double, height)
	RW_PROPERTY(GanvDirection, direction);

	Gtk::Layout& widget();

	/** Get the edge from @c tail to @c head if one exists. */
	Edge* get_edge(Node* tail, Node* head) const;

	/** Delete the edge from @c tail to @c head. */
	void remove_edge(Node* tail, Node* head);

	typedef void (*EdgePtrFunc)(GanvEdge* edge, void* data);

	void for_each_edge(EdgePtrFunc f, void* data);
	void for_each_selected_edge(EdgePtrFunc f, void* data);

	void get_scroll_offsets(int& cx, int& cy) const;
	void scroll_to(int x, int y);

	GQuark wrapper_key();

	GanvItem* root();

	GanvCanvas*       gobj();
	const GanvCanvas* gobj() const;

	sigc::signal<bool, GdkEvent*>    signal_event;
	sigc::signal<void, Node*, Node*> signal_connect;
	sigc::signal<void, Node*, Node*> signal_disconnect;

private:
	Canvas(const Canvas&);  ///< Noncopyable
	const Canvas& operator=(const Canvas&);  ///< Noncopyable

	inline       GanvCanvasImpl* impl()       { return _gobj->impl; }
	inline const GanvCanvasImpl* impl() const { return _gobj->impl; }

	GanvCanvas* const _gobj;
};

} // namespace Ganv

#endif // GANV_CANVAS_HPP
