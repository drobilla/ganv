# Copyright 2021-2022 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR GPL-3.0-or-later

project('ganv', ['c', 'cpp'],
        version: '1.8.3',
        license: 'GPLv3+',
        meson_version: '>= 0.56.0',
        default_options: [
          'b_ndebug=if-release',
          'buildtype=release',
          'c_std=c99',
          'cpp_std=c++11',
        ])

ganv_src_root = meson.current_source_dir()
major_version = meson.project_version().split('.')[0]
version_suffix = '-@0@'.format(major_version)
versioned_name = 'ganv' + version_suffix

#######################
# Compilers and Flags #
#######################

# Required tools
pkg = import('pkgconfig')
gnome = import('gnome')
cc = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')

# Set global warning flags
if get_option('strict') and not meson.is_subproject()
  subdir('meson/warnings')
endif

# Set global warning suppressions
subdir('meson/suppressions')
add_project_arguments(c_suppressions, language: ['c'])
if is_variable('cpp')
  add_project_arguments(cpp_suppressions, language: ['cpp'])
endif

################
# Dependencies #
################

m_dep = cc.find_library('m', required: false)

intl_dep = cc.find_library('intl', required: get_option('nls'))

gtk2_dep = dependency('gtk+-2.0',
                      version: '>= 2.10.0',
                      include_type: 'system')

gtkmm2_dep = dependency('gtkmm-2.4',
                        version: '>= 2.10.0',
                        include_type: 'system')

gvc_dep = dependency('libgvc',
                     include_type: 'system',
                     required: get_option('graphviz'))

##########################
# Platform Configuration #
##########################

# Use versioned name everywhere to support parallel major version installations
if host_machine.system() == 'windows'
  if get_option('default_library') == 'both'
    error('default_library=both is not supported on Windows')
  endif
  soversion = ''
else
  soversion = meson.project_version().split('.')[0]
endif

#################
# Configuration #
#################

config_defines = []

# Graphviz layout
if gvc_dep.found()
  config_defines += ['-DHAVE_AGRAPH']
endif

# Force-directed graph layout
if not get_option('fdgl').disabled()
  config_defines += ['-DGANV_FDGL']
endif

# Light theme
if get_option('light')
  config_defines += ['-DGANV_USE_LIGHT_THEME']
endif

# Native language support
if not get_option('nls').disabled()
  dgettext_code = '''#include <libintl.h>
int main(void) { !!dgettext("ganv", "string"); }'''

  config_defines += ['-DHAVE_DGETTEXT=@0@'.format(
    cc.compiles(dgettext_code, name: 'dgettext').to_int())]
endif

add_project_arguments(config_defines, language: ['c', 'cpp'])

###########
# Library #
###########

include_dirs = include_directories('include')

c_headers = files(
  'include/ganv/box.h',
  'include/ganv/canvas.h',
  'include/ganv/circle.h',
  'include/ganv/edge.h',
  'include/ganv/ganv.h',
  'include/ganv/group.h',
  'include/ganv/item.h',
  'include/ganv/module.h',
  'include/ganv/node.h',
  'include/ganv/port.h',
  'include/ganv/text.h',
  'include/ganv/types.h',
  'include/ganv/widget.h',
)

cpp_headers = files(
  'include/ganv/Box.hpp',
  'include/ganv/Canvas.hpp',
  'include/ganv/Circle.hpp',
  'include/ganv/Edge.hpp',
  'include/ganv/Item.hpp',
  'include/ganv/Module.hpp',
  'include/ganv/Node.hpp',
  'include/ganv/Port.hpp',
  'include/ganv/ganv.hpp',
  'include/ganv/types.hpp',
  'include/ganv/wrap.hpp',
)

sources = files(
  'src/Canvas.cpp',
  'src/Port.cpp',
  'src/box.c',
  'src/circle.c',
  'src/edge.c',
  'src/group.c',
  'src/item.c',
  'src/module.c',
  'src/node.c',
  'src/port.c',
  'src/text.c',
  'src/widget.c',
)

# Set appropriate arguments for building against the library type
extra_args = []
if get_option('default_library') == 'static'
  extra_args = ['-DGANV_STATIC']
endif

# Generate marshal files with glib-genmarshal
ganv_marshal_sources = gnome.genmarshal(
  'ganv-marshal',
  extra_args: ['--quiet'],
  prefix: 'ganv_marshal',
  sources: files('src/ganv-marshal.list'),
)

# Build shared and/or static library
libganv = library(
  versioned_name,
  sources + ganv_marshal_sources,
  c_args: c_suppressions + extra_args + ['-DGANV_INTERNAL'],
  cpp_args: cpp_suppressions + extra_args + ['-DGANV_INTERNAL'],
  dependencies: [gtk2_dep, gtkmm2_dep, gvc_dep, intl_dep, m_dep],
  include_directories: include_dirs,
  install: true,
  soversion: soversion,
  version: meson.project_version(),
)

# Declare dependency for internal meson dependants
ganv_dep = declare_dependency(
  compile_args: extra_args,
  include_directories: include_dirs,
  link_with: libganv,
)

# Generage pkg-config file for external dependants
ganv_pc = pkg.generate(
  libganv,
  description: 'Interactive Gtk canvas widget for graph-based interfaces',
  extra_cflags: extra_args,
  filebase: versioned_name,
  name: 'Ganv',
  subdirs: [versioned_name],
  version: meson.project_version(),
)

# Install headers to a versioned include directory
install_headers(c_headers + cpp_headers, subdir: versioned_name / 'ganv')

# Generate GObject introspection data
if not get_option('gir').disabled()
  # TODO: With newer g-ir-scanner we could simply do
  # ['--compiler=' + cpp.cmd_array()[0]]
  g_ir_scanner_args = ['--quiet']
  if target_machine.system() == 'darwin'
    g_ir_scanner_args += ['-lc++']
  elif target_machine.system() in ['gnu', 'linux']
    g_ir_scanner_args += ['-lstdc++']
  endif

  gnome.generate_gir(
    libganv,
    dependencies: [ganv_dep],
    extra_args: g_ir_scanner_args,
    fatal_warnings: true,
    header: 'ganv.h',
    includes: ['GObject-2.0', 'Gdk-2.0', 'Gtk-2.0'],
    install: true,
    namespace: 'Ganv',
    nsversion: '1.0',
    sources: sources + c_headers + ganv_marshal_sources,
  )
endif

if not meson.is_subproject()
  summary('Tools', get_option('tools'), bool_yn: true)
  summary('Install prefix', get_option('prefix'))
  summary('Headers', get_option('prefix') / get_option('includedir'))
  summary('Libraries', get_option('prefix') / get_option('libdir'))

  if get_option('tools')
    summary('Executables', get_option('prefix') / get_option('bindir'))
  endif
endif
