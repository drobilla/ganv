#!/usr/bin/env python

from gi.repository import Ganv, Gtk

win = Gtk.Window()
win.set_title("Ganv Python Test")
win.connect("destroy", lambda obj: Gtk.main_quit())

canvas = Ganv.Canvas.new(1024, 768)
module = Ganv.Module(canvas=canvas,
                     label="Test")
# iport = Ganv.Port(module=module,
#                   is_input=True,
#                   label="In")
# oport = Ganv.Port(module=module,
#                   is_input=False,
#                   label="In")

win.add(canvas)
win.show_all()
win.present()
Gtk.main()
