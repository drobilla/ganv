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

#ifndef GANV_BOX_H
#define GANV_BOX_H

#include "ganv/node.h"

G_BEGIN_DECLS

#define GANV_TYPE_BOX            (ganv_box_get_type())
#define GANV_BOX(obj)            (GTK_CHECK_CAST((obj), GANV_TYPE_BOX, GanvBox))
#define GANV_BOX_CLASS(klass)    (GTK_CHECK_CLASS_CAST((klass), GANV_TYPE_BOX, GanvBoxClass))
#define GANV_IS_BOX(obj)         (GTK_CHECK_TYPE((obj), GANV_TYPE_BOX))
#define GANV_IS_BOX_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GANV_TYPE_BOX))
#define GANV_BOX_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS((obj), GANV_TYPE_BOX, GanvBoxClass))

typedef struct _GanvBox      GanvBox;
typedef struct _GanvBoxClass GanvBoxClass;

typedef struct {
	double   x1, y1, x2, y2;
	double   border_width;
	gboolean stacked;
} GanvBoxCoords;

struct _GanvBox
{
	GanvNode      node;
	GanvBoxCoords coords;
	GanvBoxCoords old_coords;
	double        radius_tl;
	double        radius_tr;
	double        radius_br;
	double        radius_bl;
};

struct _GanvBoxClass {
	GanvNodeClass parent_class;

	void (*set_width)(GanvBox* box,
	                  double   width);

	void (*set_height)(GanvBox* box,
	                   double   height);
};

GType ganv_box_get_type(void);

static inline double
ganv_box_get_x1(const GanvBox* box)
{
	return box->coords.x1;
}

static inline double
ganv_box_get_y1(const GanvBox* box)
{
	return box->coords.y1;
}

static inline double
ganv_box_get_x2(const GanvBox* box)
{
	return box->coords.x2;
}

static inline double
ganv_box_get_y2(const GanvBox* box)
{
	return box->coords.y2;
}

static inline double
ganv_box_get_width(const GanvBox* box)
{
	return box->coords.x2 - box->coords.x1;
}

static inline void
ganv_box_set_width(GanvBox* box,
                   double   width)
{
	GANV_BOX_GET_CLASS(box)->set_width(box, width);
}

static inline double
ganv_box_get_height(const GanvBox* box)
{
	return box->coords.y2 - box->coords.y1;
}

static inline void
ganv_box_set_height(GanvBox* box,
                    double   height)
{
	GANV_BOX_GET_CLASS(box)->set_height(box, height);
}

static inline double
ganv_box_get_border_width(const GanvBox* box)
{
	return box->coords.border_width;
}

void
ganv_box_normalize(GanvBox* box);

G_END_DECLS

#endif  /* GANV_BOX_H */
