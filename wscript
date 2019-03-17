#!/usr/bin/env python

import os

from waflib import Options, Utils
from waflib.extras import autowaf

# Library and package version (UNIX style major, minor, micro)
# major increment <=> incompatible changes
# minor increment <=> compatible changes (additions)
# micro increment <=> no interface changes
GANV_VERSION       = '1.5.4'
GANV_MAJOR_VERSION = '1'

# Mandatory waf variables
APPNAME = 'ganv'        # Package name for waf dist
VERSION = GANV_VERSION  # Package version for waf dist
top     = '.'           # Source directory
out     = 'build'       # Build directory

def options(ctx):
    ctx.load('compiler_c')
    ctx.load('compiler_cxx')
    ctx.add_flags(
        ctx.configuration_options(),
        {'no-graphviz': 'do not compile with graphviz support',
         'light-theme': 'use light coloured theme',
         'no-fdgl': 'use experimental force-directed graph layout',
         'no-nls': 'disable i18n (native language support)',
         'gir': 'build GObject introspection data'})

def configure(conf):
    conf.load('compiler_c', cache=True)
    conf.load('compiler_cxx', cache=True)
    conf.load('autowaf', cache=True)
    autowaf.set_c_lang(conf, 'c99')

    autowaf.check_pkg(conf, 'gtk+-2.0', uselib_store='GTK',
                      atleast_version='2.0.0', system=True, mandatory=True)
    autowaf.check_pkg(conf, 'gtkmm-2.4', uselib_store='GTKMM',
                      atleast_version='2.10.0', system=True, mandatory=True)

    if Options.options.gir:
        autowaf.check_pkg(conf, 'gobject-introspection-1.0',
                          uselib_store='GIR', mandatory=False)
        conf.find_program('g-ir-doc-tool', var='G_IR_DOC_TOOL', mandatory=False)
        conf.find_program('yelp-build', var='YELP_BUILD', mandatory=False)

    if not Options.options.no_graphviz:
        autowaf.check_pkg(conf, 'libgvc', uselib_store='AGRAPH',
                          atleast_version='2.30', system=True, mandatory=False)

    if not Options.options.no_fdgl:
        autowaf.define(conf, 'GANV_FDGL', 1)

    if Options.options.light_theme:
        autowaf.define(conf, 'GANV_USE_LIGHT_THEME', 1)

    if not Options.options.no_nls:
        autowaf.check_function(conf, 'cxx',  'dgettext',
                               header_name = 'libintl.h',
                               lib         = 'intl',
                               define_name = 'ENABLE_NLS',
                               mandatory   = False)

    autowaf.set_lib_env(conf, 'ganv', GANV_VERSION)
    conf.write_config_header('ganv_config.h', remove=False)

    autowaf.display_summary(
        conf,
        {'Static (Graphviz) arrange': bool(conf.env.HAVE_AGRAPH_2_20),
         'Interactive force-directed arrange': bool(conf.env.GANV_FDGL),
         'Native language support': bool(conf.env.ENABLE_NLS),
         'GObject introspection': bool(conf.env.HAVE_GIR),
         'Unit tests': bool(conf.env.BUILD_TESTS)})

ganv_source = [
    'src/Canvas.cpp',
    'src/Port.cpp',
    'src/box.c',
    'src/circle.c',
    'src/edge.c',
    'src/ganv-marshal.c',
    'src/group.c',
    'src/item.c',
    'src/module.c',
    'src/node.c',
    'src/port.c',
    'src/text.c',
    'src/widget.c'
]

def declare_doc_files(task):
    bld  = task.generator.bld
    path = bld.path.get_bld().find_or_declare('doc-html')
    for i in path.ant_glob('*', remove=False):
        i.sig = Utils.h_file(i.abspath())

def build(bld):
    # Headers
    includedir = '${INCLUDEDIR}/ganv-%s/ganv' % GANV_MAJOR_VERSION
    bld.install_files(includedir, bld.path.ant_glob('ganv/*.h*'))

    # Pkgconfig file
    autowaf.build_pc(bld, 'GANV', GANV_VERSION, GANV_MAJOR_VERSION,
                     'GTKMM AGRAPH',
                     {'GANV_MAJOR_VERSION' : GANV_MAJOR_VERSION})

    bld(rule = 'glib-genmarshal --prefix=ganv_marshal --header ${SRC} > ${TGT}',
        source = 'src/ganv-marshal.list',
        target = 'src/ganv-marshal.h')

    bld(rule = 'glib-genmarshal --prefix=ganv_marshal --body ${SRC} > ${TGT}',
        source = 'src/ganv-marshal.list',
        target = 'src/ganv-marshal.c.in')

    bld(rule = 'cat ${SRC} > ${TGT}',
        source = ['src/ganv-marshal.h', 'src/ganv-marshal.c.in'],
        target = 'src/ganv-marshal.c')

    # Library
    lib = bld(features        = 'c cshlib cxx cxxshlib',
              export_includes = ['.'],
              source          = ganv_source,
              includes        = ['.', './src'],
              name            = 'libganv',
              target          = 'ganv-%s' % GANV_MAJOR_VERSION,
              uselib          = 'GTKMM AGRAPH',
              vnum            = GANV_VERSION,
              install_path    = '${LIBDIR}')
    if bld.is_defined('ENABLE_NLS'):
        lib.lib = ['intl']

    # Benchmark program (C++)
    bld(features     = 'cxx cxxprogram',
        source       = 'src/ganv_bench.cpp',
        includes     = ['.', './src'],
        use          = 'libganv',
        uselib       = 'GTKMM AGRAPH',
        target       = 'src/ganv_bench')

    if bld.env.BUILD_TESTS:
        test_libs   = []
        test_cflags = ['']
        test_linkflags  = ['']
        if not bld.env.NO_COVERAGE:
            test_cflags    += ['--coverage']
            test_linkflags += ['--coverage']

        # Static library for test program
        bld(features     = 'c cstlib cxx cxxshlib',
            source       = ganv_source,
            includes     = ['.', './src'],
            name         = 'libganv_profiled',
            target       = 'ganv_profiled',
            uselib       = 'GTKMM AGRAPH',
            install_path = '',
            cflags       = test_cflags,
            linkflags    = test_linkflags)

        # Test program (C)
        bld(features     = 'cxx cxxprogram',
            source       = 'src/ganv_test.c',
            includes     = ['.', './src'],
            use          = 'libganv_profiled',
            lib          = test_libs,
            uselib       = 'GTKMM AGRAPH',
            target       = 'src/ganv_test',
            cflags       = test_cflags,
            linkflags    = test_linkflags)

    # Documentation
    #autowaf.build_dox(bld, 'GANV', GANV_VERSION, top, out)

    if bld.env.HAVE_GIR:
        bld.add_group()

        bld_dir = os.path.join(out, APPNAME)
        if not (len(bld.stack_path) > 1):  # not top-level
            bld_dir = out

        pc_path = os.path.abspath(os.path.join(bld_dir, 'ganv-1.pc'))

        bld(name         = 'ganv-gir',
            source       = ganv_source + bld.path.ant_glob('ganv/*.h'),
            target       = 'Ganv-1.0.gir',
            install_path = '${LIBDIR}/girepository-1.0',
            rule         = 'g-ir-scanner --warn-all -n Ganv --nsversion=1.0'
            ' --no-libtool ' +
            ('--pkg=%s' % pc_path) +
            (' -I%s' % bld.path.bldpath()) +
            ''.join([' -I' + path for path in bld.env.INCLUDES_GTK]) +
            (' -L%s' % bld_dir) +
            ' --library=ganv-1'
            ' --include=GObject-2.0 --include=Gdk-2.0 --include Gtk-2.0'
            ' -o ${TGT} ${SRC}')

        bld(name         = 'ganv-typelib',
            after        = 'ganv-gir',
            source       = 'Ganv-1.0.gir',
            target       = 'Ganv-1.0.typelib',
            install_path = '${LIBDIR}/girepository-1.0',
            rule         = 'g-ir-compiler ${SRC} -o ${TGT}')

        if bld.env.DOCS and bld.env['G_IR_DOC_TOOL'] and bld.env['YELP_BUILD']:
            # The source and target files used here aren't exclusive,
            # but are declared so waf can track dependencies
            bld(rule   = '${G_IR_DOC_TOOL} --language C -o doc-xml ${SRC}',
                source = 'Ganv-1.0.gir',
                target = 'doc-xml/index.page')
            bld(name   = 'yelp-build',
                rule   = '${YELP_BUILD} html -o doc-html doc-xml',
                source = 'doc-xml/index.page',
                target = 'doc-html/index.html')
            bld(name   = 'find-docs',
                always = True,
                rule   = declare_doc_files,
                after  = 'yelp-build')

            bld.install_files(
                os.path.join('${DOCDIR}', 'ganv-0', 'html'),
                bld.path.get_bld().ant_glob('doc-html/*'))

    bld.add_post_fun(autowaf.run_ldconfig)

def test(tst):
    tst.run(['./src/ganv_test'])

def i18n(bld):
    autowaf.build_i18n(bld, '..', 'ganv', APPNAME, ganv_source,
                       'David Robillard')

def posts(ctx):
    path = str(ctx.path.abspath())
    autowaf.news_to_posts(
        os.path.join(path, 'NEWS'),
        {'title'        : 'Ganv',
         'description'  : autowaf.get_blurb(os.path.join(path, 'README')),
         'dist_pattern' : 'http://download.drobilla.net/ganv-%s.tar.bz2'},
        { 'Author' : 'drobilla',
          'Tags'   : 'Hacking, LAD, LV2, Ganv' },
        os.path.join(out, 'posts'))
