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

#include <string>

#include <glib.h>

#include "ganv/Canvas.hpp"
#include "ganv/Module.hpp"
#include "ganv/Port.hpp"

#include "./color.h"

namespace Ganv {

static void
on_value_changed(GanvPort* port, double value, void* portmm)
{
	((Port*)portmm)->signal_value_changed.emit(value);
}

/* Construct a Port on an existing module. */
Port::Port(Module&            module,
           const std::string& name,
           bool               is_input,
           uint32_t           color)
	: Box(module.canvas(),
	      GANV_BOX(ganv_port_new(module.gobj(), is_input,
	                             "fill-color", color,
	                             "border-color", highlight_color(color, 0x20),
	                             "border-width", 2.0,
	                             "label", name.c_str(),
	                             NULL)))
{
	g_signal_connect(gobj(), "value-changed",
	                 G_CALLBACK(on_value_changed), this);
}

Module*
Port::get_module() const
{
	return Glib::wrap(ganv_port_get_module(gobj()));
}

} // namespace Ganv
