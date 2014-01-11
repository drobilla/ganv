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

#ifndef GANV_TEXT_H
#define GANV_TEXT_H

#include "ganv/item.h"

G_BEGIN_DECLS

#define GANV_TYPE_TEXT            (ganv_text_get_type())
#define GANV_TEXT(obj)            (GTK_CHECK_CAST((obj), GANV_TYPE_TEXT, GanvText))
#define GANV_TEXT_CLASS(klass)    (GTK_CHECK_CLASS_CAST((klass), GANV_TYPE_TEXT, GanvTextClass))
#define GANV_IS_TEXT(obj)         (GTK_CHECK_TYPE((obj), GANV_TYPE_TEXT))
#define GANV_IS_TEXT_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GANV_TYPE_TEXT))
#define GANV_TEXT_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS((obj), GANV_TYPE_TEXT, GanvTextClass))

typedef struct _GanvText      GanvText;
typedef struct _GanvTextClass GanvTextClass;
typedef struct _GanvTextImpl  GanvTextImpl;

struct _GanvText {
	GanvItem      item;
	GanvTextImpl* impl;
};

struct _GanvTextClass {
	GanvItemClass parent_class;
};

GType ganv_text_get_type(void);

void ganv_text_layout(GanvText* text);

G_END_DECLS

#endif  /* GANV_TEXT_H */
