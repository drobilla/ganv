/* This file is part of Ganv.
 * Copyright 2013 David Robillard <http://drobilla.net>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtkmm/main.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/window.h>

#include "ganv/ganv.hpp"

using namespace std;
using namespace Ganv;

static const int MAX_NUM_PORTS = 16;

vector<Node*> ins;
vector<Node*> outs;

static Module*
make_module(Canvas* canvas)
{
	char name[8];

	snprintf(name, 8, "mod%d", rand() % 10000);
	Module* m(new Module(*canvas, name,
	                     rand() % (int)canvas->get_width(),
	                     rand() % (int)canvas->get_height(),
	                     true));

	int n_ins = rand() % MAX_NUM_PORTS;
	for (int i = 0; i < n_ins; ++i) {
		snprintf(name, 8, "in%d", rand() % 10000);
		Port* p(new Port(*m, name, true,
		                 ((rand() % 0xFFFFFF) << 8) | 0xFF));
		ins.push_back(p);
	}

	int n_outs = rand() % MAX_NUM_PORTS;
	for (int i = 0; i < n_outs; ++i) {
		snprintf(name, 8, "out%d", rand() % 10000);
		Port* p(new Port(*m, name, false,
		                 ((rand() % 0xFFFFFF) << 8) | 0xFF));
		outs.push_back(p);
	}

	m->show();
	return m;
}

static Circle*
make_circle(Canvas* canvas)
{
	char name[8];

	snprintf(name, 8, "%d", rand() % 10000);
	Circle* e(new Circle(*canvas, name,
	                     rand() % (int)canvas->get_width(),
	                     rand() % (int)canvas->get_height()));

	ins.push_back(e);
	outs.push_back(e);

	return e;
}

static bool
quit()
{
	Gtk::Main::quit();
	return true;
}

static int
print_usage(const char* name)
{
	fprintf(stderr,
	        "USAGE: %s [OPTION]... CANVAS_W CANVAS_H N_MODULES N_CIRCLES N_EDGES\n\n"
	        "Options:\n"
	        "  -o  Remain open (do not close immediately)\n"
	        "  -a  Arrange canvas\n"
	        "  -s  Straight edges\n",
	        name);
	return 1;
}

int
main(int argc, char** argv)
{
	if (argc < 5) {
		return print_usage(argv[0]);
	}

	int arg = 1;

	bool remain_open = false;
	bool arrange     = false;
	bool straight    = false;
	for (; arg < argc && argv[arg][0] == '-'; ++arg) {
		if (argv[arg][1] == 'o') {
			remain_open = true;
		} else if (argv[arg][1] == 'a') {
			arrange = true;
		} else if (argv[arg][1] == 's') {
			straight = true;
		} else {
			return print_usage(argv[0]);
		}
	}

	const int canvas_w = atoi(argv[arg++]);
	const int canvas_h = atoi(argv[arg++]);

	if (argc - arg < 3) {
		return print_usage(argv[0]);
	}

	const int n_modules = atoi(argv[arg++]);
	const int n_circles = atoi(argv[arg++]);
	const int n_edges   = atoi(argv[arg++]);

	srand(time(NULL));

	Gtk::Main kit(argc, argv);

	Gtk::Window window;
	Gtk::ScrolledWindow* scroller = Gtk::manage(new Gtk::ScrolledWindow());

	Canvas* canvas = new Canvas(canvas_w, canvas_h);
	scroller->add(canvas->widget());
	window.add(*scroller);

	window.show_all();

	for (int i = 0; i < n_modules; ++i) {
		make_module(canvas);
	}

	for (int i = 0; i < n_circles; ++i) {
		make_circle(canvas);
	}

	for (int i = 0; i < n_edges; ++i) {
		Node* src = outs[rand() % outs.size()];
		Node* dst = ins[rand() % ins.size()];
		Edge* c = new Edge(*canvas, src, dst, 0x808080FF);
		if (straight) {
			c->set_curved(false);
		}
	}

	if (arrange) {
		canvas->arrange();
	}

	if (!remain_open) {
		Glib::signal_idle().connect(sigc::ptr_fun(quit));
	}

	Gtk::Main::run(window);

	return 0;
}
