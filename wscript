#!/usr/bin/env python
import os
import waflib.Options as Options
from waflib.extras import autowaf as autowaf

# Version of this package (even if built as a child)
GANV_VERSION       = '1.2.0'
GANV_MAJOR_VERSION = '1'

# Library version (UNIX style major, minor, micro)
# major increment <=> incompatible changes
# minor increment <=> compatible changes (additions)
# micro increment <=> no interface changes
# Ganv as of 1.0.0 uses the same version number for both library and package
GANV_LIB_VERSION = GANV_VERSION

# Variables for 'waf dist'
APPNAME = 'ganv'
VERSION = GANV_VERSION

# Mandatory variables
top = '.'
out = 'build'

def options(opt):
    opt.load('compiler_c')
    opt.load('compiler_cxx')
    autowaf.set_options(opt)
    opt.add_option('--test', action='store_true', default=False, dest='build_tests',
                   help="Build unit tests")
    opt.add_option('--no-graphviz', action='store_true', default=False,
                   dest='no_graphviz',
                   help='Do not compile with graphviz support')
    opt.add_option('--no-nls', action='store_true', default=False, dest='no_nls',
                   help='Disable i18n (native language support)')
    opt.add_option('--gir', action='store_true', default=False, dest='gir',
                   help='Build GObject introspection data')

def configure(conf):
    conf.load('compiler_c')
    conf.load('compiler_cxx')
    autowaf.configure(conf)
    conf.line_just = 44
    autowaf.display_header('Ganv Configuration')

    conf.env.append_unique('CFLAGS', '-std=c99')
    conf.env['BUILD_TESTS'] = Options.options.build_tests

    autowaf.check_pkg(conf, 'gtk+-2.0', uselib_store='GTK',
                      atleast_version='2.0.0', mandatory=True)
    autowaf.check_pkg(conf, 'gtkmm-2.4', uselib_store='GTKMM',
                      atleast_version='2.10.0', mandatory=True)
    autowaf.check_pkg(conf, 'libgnomecanvas-2.0', uselib_store='GNOMECANVAS',
                      atleast_version='2.0.0', mandatory=True)

    if Options.options.gir:
        autowaf.check_pkg(conf, 'gobject-introspection-1.0',
                          uselib_store='GIR', mandatory=False)

    if not Options.options.no_graphviz:
        autowaf.check_pkg(conf, 'libgvc', uselib_store='AGRAPH',
                          atleast_version='2.8', mandatory=False)

    if not Options.options.no_nls:
        autowaf.check_header(conf, 'c', 'libintl.h', 'ENABLE_NLS', mandatory=False)

    conf.env['LIB_GANV'] = ['ganv-%s' % GANV_MAJOR_VERSION]

    conf.write_config_header('ganv-config.h', remove=False)

    autowaf.display_msg(conf, "Auto-arrange", conf.is_defined('HAVE_AGRAPH'))
    autowaf.display_msg(conf, "Native language support", conf.is_defined('ENABLE_NLS'))
    autowaf.display_msg(conf, "GObject introspection", conf.is_defined('HAVE_GIR'))
    autowaf.display_msg(conf, "Unit tests", str(conf.env['BUILD_TESTS']))
    print('')

ganv_source = [
    'src/Canvas.cpp',
    'src/Port.cpp',
    'src/box.c',
    'src/circle.c',
    'src/edge.c',
    'src/module.c',
    'src/node.c',
    'src/port.c',
    'src/text.c'
]

def build(bld):
    # Headers
    includedir = '${INCLUDEDIR}/ganv-%s/ganv' % GANV_MAJOR_VERSION
    bld.install_files(includedir, bld.path.ant_glob('ganv/*.h*'))

    # Pkgconfig file
    autowaf.build_pc(bld, 'GANV', GANV_VERSION, GANV_MAJOR_VERSION,
                     'AGRAPH GLIBMM GNOMECANVAS',
                     {'GANV_MAJOR_VERSION' : GANV_MAJOR_VERSION})

    # Library
    obj = bld(features = 'c cshlib cxx cxxshlib')
    obj.export_includes = ['.']
    obj.source          = ganv_source
    obj.includes        = ['.', './src']
    obj.name            = 'libganv'
    obj.target          = 'ganv-%s' % GANV_MAJOR_VERSION
    obj.uselib          = 'GTKMM GNOMECANVAS AGRAPH'
    obj.vnum            = GANV_LIB_VERSION
    obj.install_path    = '${LIBDIR}'

    # Benchmark program (C++)
    obj = bld(features     = 'cxx cxxprogram',
              source       = 'src/ganv_bench.cpp',
              includes     = ['.', './src'],
              use          = 'libganv',
              use_lib      = 'GTKMM',
              target       = 'src/ganv_bench')

    if bld.env['BUILD_TESTS']:
        # Static library for test program
        obj = bld(features     = 'c cstlib',
                  source       = ganv_source,
                  includes     = ['.', './src'],
                  name         = 'libganv_profiled',
                  target       = 'ganv_profiled',
                  uselib       = 'GTKMM GNOMECANVAS AGRAPH',
                  install_path = '',
                  cflags       = [ '-fprofile-arcs', '-ftest-coverage' ])

        # Test program (C)
        obj = bld(features     = 'c cprogram',
                  source       = 'src/ganv_test.c',
                  includes     = ['.', './src'],
                  use          = 'libganv_profiled',
                  lib          = ['gcov'],
                  use_lib      = 'GTK',
                  target       = 'src/ganv_test')

    # Documentation
    autowaf.build_dox(bld, 'GANV', GANV_VERSION, top, out)

    if bld.is_defined('HAVE_GIR'):
        bld.add_group()

        top_level = (len(bld.stack_path) > 1)
        bld_dir   = os.path.join(out, APPNAME) if top_level else out
        pc_path   = os.path.abspath(os.path.join(bld_dir, 'ganv-1.pc'))

        gir = bld(
            name         = 'ganv-gir',
            source       = ganv_source + bld.path.ant_glob('ganv/*.h'),
            target       = 'Ganv-1.0.gir',
            install_path = '${LIBDIR}/girepository-1.0',
            rule         = 'g-ir-scanner --warn-all -n Ganv --nsversion=1.0'
            ' --no-libtool ' +
            ('--pkg=%s' % pc_path) +
            (' -I%s' % bld.path.bldpath()) +
            (' -L%s' % bld_dir) +
            ' --library=ganv-1'
            ' --include=GObject-2.0 --include=Gdk-2.0 --include Gtk-2.0'
            ' -o ${TGT} ${SRC}')

        typelib = bld(
            name         = 'ganv-typelib',
            after        = 'ganv-gir',
            source       = 'Ganv-1.0.gir',
            target       = 'Ganv-1.0.typelib',
            install_path = '${LIBDIR}/girepository-1.0',
            rule         = 'g-ir-compiler ${SRC} -o ${TGT}')

    bld.add_post_fun(autowaf.run_ldconfig)

def test(ctx):
    autowaf.pre_test(ctx, APPNAME)
    autowaf.run_tests(ctx, APPNAME, ['src/ganv_test'], dirs=['./src'])
    autowaf.post_test(ctx, APPNAME)

def i18n(bld):
    autowaf.build_i18n(bld, '..', 'ganv', APPNAME, ganv_source,
                       'David Robillard')
