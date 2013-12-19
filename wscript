#!/usr/bin/env python
import os
import waflib.Options as Options
import waflib.Utils as Utils
import waflib.extras.autowaf as autowaf

# Library and package version (UNIX style major, minor, micro)
# major increment <=> incompatible changes
# minor increment <=> compatible changes (additions)
# micro increment <=> no interface changes
GANV_VERSION       = '1.2.1'
GANV_MAJOR_VERSION = '1'

# Mandatory waf variables
APPNAME = 'ganv'        # Package name for waf dist
VERSION = GANV_VERSION  # Package version for waf dist
top     = '.'           # Source directory
out     = 'build'       # Build directory

def options(opt):
    opt.load('compiler_c')
    opt.load('compiler_cxx')
    autowaf.set_options(opt)
    opt.add_option('--test', action='store_true', dest='build_tests',
                   help='Build unit tests')
    opt.add_option('--no-graphviz', action='store_true', dest='no_graphviz',
                   help='Do not compile with graphviz support')
    opt.add_option('--fdgl', action='store_true', dest='fdgl',
                   help='Use experimental force-directed graph layout')
    opt.add_option('--no-nls', action='store_true', dest='no_nls',
                   help='Disable i18n (native language support)')
    opt.add_option('--gir', action='store_true', dest='gir',
                   help='Build GObject introspection data')

def configure(conf):
    conf.load('compiler_c')
    conf.load('compiler_cxx')
    autowaf.configure(conf)
    autowaf.set_c99_mode(conf)
    autowaf.display_header('Ganv Configuration')

    conf.env.BUILD_TESTS = Options.options.build_tests

    autowaf.check_pkg(conf, 'gtk+-2.0', uselib_store='GTK',
                      atleast_version='2.0.0', mandatory=True)
    autowaf.check_pkg(conf, 'gtkmm-2.4', uselib_store='GTKMM',
                      atleast_version='2.10.0', mandatory=True)

    if Options.options.gir:
        autowaf.check_pkg(conf, 'gobject-introspection-1.0',
                          uselib_store='GIR', mandatory=False)
        conf.find_program('g-ir-doc-tool', var='G_IR_DOC_TOOL', mandatory=False)
        conf.find_program('yelp-build', var='YELP_BUILD', mandatory=False)

    if not Options.options.no_graphviz:
        autowaf.check_pkg(conf, 'libgvc', uselib_store='AGRAPH',
                          atleast_version='2.8', mandatory=False)

    if Options.options.fdgl:
        autowaf.define(conf, 'GANV_FDGL', 1)

    if not Options.options.no_nls:
        autowaf.check_header(conf, 'c', 'libintl.h', 'ENABLE_NLS', mandatory=False)

    conf.env.LIB_GANV = ['ganv-%s' % GANV_MAJOR_VERSION]

    conf.write_config_header('ganv_config.h', remove=False)

    autowaf.display_msg(conf, "Auto-arrange", conf.is_defined('HAVE_AGRAPH'))
    autowaf.display_msg(conf, "Native language support", conf.is_defined('ENABLE_NLS'))
    autowaf.display_msg(conf, "GObject introspection", conf.is_defined('HAVE_GIR'))
    autowaf.display_msg(conf, "Unit tests", str(conf.env.BUILD_TESTS))
    print('')

ganv_source = [
    'src/Canvas.cpp',
    'src/Port.cpp',
    'src/box.c',
    'src/canvas-base.c',
    'src/circle.c',
    'src/edge.c',
    'src/ganv-marshal.c',
    'src/group.c',
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
                     'AGRAPH GLIBMM ART',
                     {'GANV_MAJOR_VERSION' : GANV_MAJOR_VERSION})

    bld(rule = 'glib-genmarshal --prefix=ganv_marshal --header ${SRC} > ${TGT}',
        source = 'src/ganv-marshal.list',
        target = 'src/ganv-marshal.h')

    bld(rule = 'glib-genmarshal --prefix=ganv_marshal --body ${SRC} > ${TGT}',
        source = 'src/ganv-marshal.list',
        target = 'src/ganv-marshal.c')

    # Library
    bld(features        = 'c cshlib cxx cxxshlib',
        export_includes = ['.'],
        source          = ganv_source,
        includes        = ['.', './src'],
        name            = 'libganv',
        target          = 'ganv-%s' % GANV_MAJOR_VERSION,
        uselib          = 'GTKMM AGRAPH ART',
        vnum            = GANV_VERSION,
        install_path    = '${LIBDIR}')

    # Benchmark program (C++)
    bld(features     = 'cxx cxxprogram',
        source       = 'src/ganv_bench.cpp',
        includes     = ['.', './src'],
        use          = 'libganv',
        use_lib      = 'GTKMM',
        target       = 'src/ganv_bench')

    if bld.env.BUILD_TESTS:
        # Static library for test program
        bld(features     = 'c cstlib cxx cxxshlib',
            source       = ganv_source,
            includes     = ['.', './src'],
            name         = 'libganv_profiled',
            target       = 'ganv_profiled',
            uselib       = 'GTKMM AGRAPH ART',
            install_path = '',
            cflags       = [ '-fprofile-arcs', '-ftest-coverage' ])

        # Test program (C)
        bld(features     = 'cxx cxxprogram',
            source       = 'src/ganv_test.c',
            includes     = ['.', './src'],
            use          = 'libganv_profiled',
            lib          = ['gcov'],
            use_lib      = 'GTK',
            target       = 'src/ganv_test')

    # Documentation
    #autowaf.build_dox(bld, 'GANV', GANV_VERSION, top, out)

    if bld.is_defined('HAVE_GIR'):
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

def test(ctx):
    autowaf.pre_test(ctx, APPNAME)
    autowaf.run_tests(ctx, APPNAME, ['src/ganv_test'], dirs=['./src'])
    autowaf.post_test(ctx, APPNAME)

def i18n(bld):
    autowaf.build_i18n(bld, '..', 'ganv', APPNAME, ganv_source,
                       'David Robillard')
