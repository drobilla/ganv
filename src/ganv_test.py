#!/usr/bin/env python

from gi.repository import Ganv, Gtk

win = Gtk.Window()
win.set_title("Ganv Python Test")
win.connect("destroy", lambda obj: Gtk.main_quit())

canvas = Ganv.Canvas.new(1024, 768)

win.add(canvas)
win.show_all()
win.present()
Gtk.main()
