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

#ifndef GANV_EDGE_HPP
#define GANV_EDGE_HPP

#include <stdint.h>

#include <gdk/gdkevents.h>

#include "ganv/canvas-base.h"
#include "ganv/Canvas.hpp"
#include "ganv/Item.hpp"
#include "ganv/Node.hpp"
#include "ganv/edge.h"

GANV_GLIB_WRAP(Edge)

namespace Ganv {

class Canvas;

/** A edge (line) between two Node objects.
 *
 * @ingroup Ganv
 */
class Edge : public Item
{
public:
	Edge(Canvas&  canvas,
	     Node*    tail,
	     Node*    head,
	     uint32_t color,
	     bool     show_arrowhead = false,
	     bool     curved = true)
		: Item(GANV_ITEM(
				       ganv_edge_new(
					       canvas.gobj(),
					       tail->gobj(),
					       head->gobj(),
					       "color", color,
					       "curved", (gboolean)curved,
					       "arrowhead", (gboolean)show_arrowhead,
					       NULL)))
	{
	}

	Edge(GanvEdge* gobj)
		: Item(GANV_ITEM(gobj))
	{}

	virtual ~Edge() {
		if (_gobj && _gobj->parent) {
			g_object_unref(_gobj);
		}
	}

	/** Return true iff the handle is within the given rectangle. */
	virtual gboolean is_within(double x1, double y1,
	                           double x2, double y2) const {
		return ganv_edge_is_within(gobj(), x1, y1, x2, y2);
	}

	RW_PROPERTY(gboolean, curved)
	RW_PROPERTY(gboolean, selected)
	RW_PROPERTY(gboolean, highlighted)
	RW_PROPERTY(guint,    color)

	METHODRETWRAP0(ganv_edge, Node*, get_tail);
	METHODRETWRAP0(ganv_edge, Node*, get_head);

	METHOD1(ganv_edge, tick, double, seconds);

	GanvEdge*       gobj()       { return (GanvEdge*)_gobj; }
	const GanvEdge* gobj() const { return (GanvEdge*)_gobj; }

private:
	Edge(const Edge& copy);
	Edge& operator=(const Edge& other);
};

} // namespace Ganv

#endif // GANV_EDGE_HPP
