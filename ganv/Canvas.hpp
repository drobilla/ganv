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
 * A generic dataflow widget using libgnomecanvas.
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

	Gtk::Layout& widget();

	/** Remove all ports and edges and modules. */
	void destroy();

	/** Get the edge from @c tail to @c head if one exists. */
	Edge* get_edge(Node* tail, Node* head) const;

	/** Delete the edge from @c tail to @c head. */
	void remove_edge(Node* tail, Node* head);

	/** Place @c i at a reasonable default location. */
	void set_default_placement(Node* i);

	/** Unselect all selected objects. */
	virtual void clear_selection();

	void select_all();

	RW_PROPERTY(gboolean, locked);

	/** Return the current zoom factor (pixels per unit). */
	double get_zoom();

	/** Set the current zoom factor (pixels per unit). */
	void set_zoom(double pix_per_unit);

	/** Zoom so all canvas contents are visible. */
	void zoom_full();

	METHODRET0(ganv_canvas, double, get_font_size)
	METHODRET0(ganv_canvas, double, get_default_font_size)

	/** Set the current font size. */
	void set_font_size(double points);

	/** Set both the zoom factor and font size. */
	void set_zoom_and_font_size(double zoom, double points);

	/** Write a Graphviz DOT description of the canvas to @c filename. */
	void render_to_dot(const std::string& filename);

	/** Automatically arrange the canvas contents if Graphviz is available. */
	void arrange(bool use_length_hints=false);

	/** Shift all canvas contents so the top-left object is at (x, y). */
	void move_contents_to(double x, double y);

	RW_PROPERTY(double, width)
	RW_PROPERTY(double, height)

	/** Resize the canvas to the given dimensions. */
	void resize(double width, double height);

	RW_PROPERTY(GanvDirection, direction);

	typedef void (*NodeFunction)(GanvNode* node, void* data);

	void for_each_node(NodeFunction f, void* data);
	void for_each_selected_node(NodeFunction f, void* data);

	typedef void (*EdgePtrFunction)(GanvEdge* edge, void* data);

	void for_each_edge(EdgePtrFunction f, void* data);
	void for_each_selected_edge(EdgePtrFunction f, void* data);

	METHOD2(ganv_canvas, for_each_edge_from,
	        const GanvNode*, tail,
	        GanvEdgeFunction, f);

	METHOD2(ganv_canvas, for_each_edge_to,
	        const GanvNode*, head,
	        GanvEdgeFunction, f);

	METHOD2(ganv_canvas, for_each_edge_on,
	        const GanvNode*, node,
	        GanvEdgeFunction, f);

	GQuark wrapper_key();

	GnomeCanvasGroup* root();

	GdkCursor* move_cursor();

	void get_scroll_offsets(int& cx, int& cy) const;
	void scroll_to(int x, int y);

	GanvCanvas*       gobj();
	const GanvCanvas* gobj() const;

	sigc::signal<bool, GdkEvent*>    signal_event;
	sigc::signal<void, Node*, Node*> signal_connect;
	sigc::signal<void, Node*, Node*> signal_disconnect;

	/** Signal emitted when the mouse pointer enters an Item. */
	sigc::signal<void, GnomeCanvasItem*> signal_item_entered;

	/** Signal emitted when the mouse pointer leaves an Item. */
	sigc::signal<void, GnomeCanvasItem*> signal_item_left;

private:
	Canvas(const Canvas&);  ///< Noncopyable
	const Canvas& operator=(const Canvas&);  ///< Noncopyable

	inline       GanvCanvasImpl* impl()       { return _gobj->impl; }
	inline const GanvCanvasImpl* impl() const { return _gobj->impl; }

	GanvCanvas* const _gobj;
};

} // namespace Ganv

#endif // GANV_CANVAS_HPP
