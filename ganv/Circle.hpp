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

#ifndef GANV_CIRCLE_HPP
#define GANV_CIRCLE_HPP

#include <algorithm>
#include <map>
#include <string>
#include <stdint.h>

#include <gdkmm/types.h>

#include "ganv/types.hpp"
#include "ganv/Node.hpp"
#include "ganv/circle.h"

GANV_GLIB_WRAP(Circle)

namespace Ganv {

class Canvas;

/** An elliptical Item which is Node.
 *
 * Unlike a Module, this doesn't contain ports, but is directly joinable itself
 * (think your classic circles 'n' lines diagram, ala FSM).
 *
 * @ingroup Ganv
 */
class Circle : public Node
{
public:
	static const uint32_t FILL_COLOUR   = 0x1E2224FF;
	static const uint32_t BORDER_COLOUR = 0xD3D7CFFF;

	Circle(Canvas&            canvas,
	       const std::string& name,
	       double             x,
	       double             y,
	       double             radius,
	       bool               show_title)
		: Node(&canvas,
		       GANV_NODE(
			       ganv_item_new(
				       GANV_ITEM(canvas.root()),
				       ganv_circle_get_type(),
				       "x", x,
				       "y", y,
				       "can-tail", TRUE,
				       "can-head", TRUE,
				       "radius", radius,
				       "fill-color", FILL_COLOUR,
				       "border-color", BORDER_COLOUR,
				       "label", name.c_str(),
				       "draggable", TRUE,
				       NULL)))
	{}

	RW_PROPERTY(double, radius);

	GanvCircle*       gobj()       { return GANV_CIRCLE(_gobj); }
	const GanvCircle* gobj() const { return GANV_CIRCLE(_gobj); }
};

} // namespace Ganv

#endif // GANV_CIRCLE_HPP
