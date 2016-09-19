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

#ifndef GANV_UTIL_H
#define GANV_UTIL_H

#include <glib.h>

#ifdef GANV_USE_LIGHT_THEME
#    define DEFAULT_BACKGROUND_COLOR 0xFFFFFFFF
#    define DEFAULT_TEXT_COLOR       0x000000FF
#    define DIM_TEXT_COLOR           0x333333BB
#    define DEFAULT_FILL_COLOR       0xEEEEEEFF
#    define DEFAULT_BORDER_COLOR     0x000000FF
#    define PORT_BORDER_COLOR(fill)  0x000000FF
#    define EDGE_COLOR(base)         highlight_color(tail_color, -48)
#else
#    define DEFAULT_BACKGROUND_COLOR 0x000000FF
#    define DEFAULT_TEXT_COLOR       0xFFFFFFFF
#    define DIM_TEXT_COLOR           0xCCCCCCBB
#    define DEFAULT_FILL_COLOR       0x1E2224FF
#    define DEFAULT_BORDER_COLOR     0x3E4244FF
#    define PORT_BORDER_COLOR(fill)  highlight_color(fill, 0x20)
#    define EDGE_COLOR(base)         highlight_color(tail_color, 48)
#endif

static inline void
color_to_rgba(guint color, double* r, double* g, double* b, double* a)
{
	*r = ((color >> 24) & 0xFF) / 255.0;
	*g = ((color >> 16) & 0xFF) / 255.0;
	*b = ((color >> 8)  & 0xFF) / 255.0;
	*a = ((color)       & 0xFF) / 255.0;
}

static inline guint
highlight_color(guint c, guint delta)
{
	const guint max_char = 255;
	const guint r        = MIN((c >> 24) + delta, max_char);
	const guint g        = MIN(((c >> 16) & 0xFF) + delta, max_char);
	const guint b        = MIN(((c >> 8) & 0xFF) + delta, max_char);
	const guint a        = c & 0xFF;

	return ((((guint)(r)) << 24) |
	        (((guint)(g)) << 16) |
	        (((guint)(b)) << 8) |
	        (((guint)(a))));
}

#endif  // GANV_UTIL_H
