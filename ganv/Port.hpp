/* This file is part of Ganv.
 * Copyright 2007-2011 David Robillard <http://drobilla.net>
 *
 * Ganv is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Ganv is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Ganv.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GANV_PORT_HPP
#define GANV_PORT_HPP

#include <stdint.h>

#include <string>

#include <libgnomecanvas/gnome-canvas.h>
#include <gdkmm/types.h>

#include "ganv/Box.hpp"
#include "ganv/port.h"

GANV_GLIB_WRAP(Port)

namespace Ganv {

class Module;

/** A port on a Module.
 *
 * This is a group that contains both the label and rectangle for a port.
 *
 * @ingroup Ganv
 */
class Port : public Box
{
public:
	Port(Module&            module,
	     const std::string& name,
	     bool               is_input,
	     uint32_t           color);

	METHODRET0(ganv_port, gboolean, is_input)
	METHODRET0(ganv_port, gboolean, is_output)

	METHODRET0(ganv_port, double, get_natural_width);
	METHODRET0(ganv_port, float, get_control_value)
	METHODRET0(ganv_port, float, get_control_min)
	METHODRET0(ganv_port, float, get_control_max)
	METHOD0(ganv_port, show_control)
	METHOD0(ganv_port, hide_control)
	METHOD1(ganv_port, set_control_is_toggle, gboolean, is_toggle)
	METHOD1(ganv_port, set_control_value, float, value)
	METHOD1(ganv_port, set_control_min, float, min)
	METHOD1(ganv_port, set_control_max, float, max)

	Module* get_module() const;

	GanvPort*       gobj()       { return GANV_PORT(_gobj); }
	const GanvPort* gobj() const { return GANV_PORT(_gobj); }
};

} // namespace Ganv

#endif // GANV_PORT_HPP
