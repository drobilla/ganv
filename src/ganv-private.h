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

#ifndef GANV_PRIVATE_H
#define GANV_PRIVATE_H

void
ganv_canvas_add_node(GanvCanvas* canvas,
                     GanvNode*   node);

void
ganv_canvas_remove_node(GanvCanvas* canvas,
                        GanvNode*   node);

void
ganv_canvas_move_selected_items(GanvCanvas* canvas,
                                double      dx,
                                double      dy);

void
ganv_canvas_selection_move_finished(GanvCanvas* canvas);

void
ganv_canvas_select_node(GanvCanvas* canvas,
                        GanvNode*   node);

void
ganv_canvas_unselect_node(GanvCanvas* canvas,
                          GanvNode*   node);

void
ganv_canvas_add_edge(GanvCanvas* canvas,
                     GanvEdge*   edge);

void
ganv_canvas_remove_edge(GanvCanvas* canvas,
                        GanvEdge*   edge);

void
ganv_canvas_select_edge(GanvCanvas* canvas,
                        GanvEdge*   edge);

void
ganv_canvas_unselect_edge(GanvCanvas* canvas,
                          GanvEdge*   edge);

GdkCursor*
ganv_canvas_get_move_cursor(const GanvCanvas* canvas);

gboolean
ganv_canvas_port_event(GanvCanvas* canvas,
                       GanvPort*   port,
                       GdkEvent*   event);

#endif  /* GANV_PRIVATE_H */
