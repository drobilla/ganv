# Copyright 2020-2023 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR ISC

# Project-specific warning suppressions

warning_level = get_option('warning_level')

clang_common_suppressions = [
  '-Wno-cast-qual',
  '-Wno-covered-switch-default',
  '-Wno-disabled-macro-expansion',
  '-Wno-documentation-unknown-command',
  '-Wno-double-promotion',
  '-Wno-float-conversion',
  '-Wno-float-equal',
  '-Wno-implicit-fallthrough',
  '-Wno-implicit-float-conversion',
  '-Wno-padded',
  '-Wno-reserved-id-macro',
  '-Wno-reserved-identifier',
  '-Wno-shadow',
  '-Wno-shorten-64-to-32',
  '-Wno-sign-conversion',
  '-Wno-switch-default',
  '-Wno-switch-enum',
  '-Wno-unused-macros',
  '-Wno-used-but-marked-unused',
]

gcc_common_suppressions = [
  '-Wno-cast-qual',
  '-Wno-conversion',
  '-Wno-deprecated-declarations',
  '-Wno-double-promotion',
  '-Wno-float-conversion',
  '-Wno-float-equal',
  '-Wno-format',
  '-Wno-implicit-fallthrough',
  '-Wno-padded',
  '-Wno-shadow',
  '-Wno-sign-conversion',
  '-Wno-suggest-attribute=const',
  '-Wno-suggest-attribute=pure',
  '-Wno-switch-default',
  '-Wno-switch-enum',
  '-Wno-unused-macros',
]

#####
# C #
#####

if is_variable('cc')
  c_suppressions = []

  if cc.get_id() == 'clang'
    if warning_level == 'everything'
      c_suppressions += [
        '-Wno-bad-function-cast',
        '-Wno-declaration-after-statement',
        '-Wno-documentation',
        '-Wno-unsafe-buffer-usage',
      ]

      if host_machine.system() == 'freebsd'
        c_suppressions += [
          '-Wno-c11-extensions',
        ]
      endif
    endif

    if warning_level in ['everything', '3']
      c_suppressions += (
        clang_common_suppressions + [
          '-Wno-cast-function-type',
          '-Wno-pedantic',
        ]
      )
    endif

  elif cc.get_id() == 'gcc'
    if warning_level == 'everything'
      c_suppressions += [
        '-Wno-bad-function-cast',
        '-Wno-c++-compat',
        '-Wno-unsuffixed-float-constants',
      ]
    endif

    if warning_level in ['everything', '3']
      c_suppressions += (
        gcc_common_suppressions + [
          '-Wno-pedantic',
        ]
      )
    endif

    if warning_level in ['everything', '3', '2']
      c_suppressions += (
        gcc_common_suppressions + [
          '-Wno-cast-function-type',
        ]
      )
    endif

  elif cc.get_id() == 'msvc'
    if warning_level == 'everything'
      c_suppressions += [
        '/wd4061', # enumerator in switch is not explicitly handled
        '/wd4100', # unreferenced formal parameter
        '/wd4191', # unsafe function conversion
        '/wd4200', # zero-sized array in struct/union
        '/wd4244', # possible loss of data from integer conversion
        '/wd4267', # possible loss of data from size conversion
        '/wd4365', # signed/unsigned mismatch
        '/wd4514', # unreferenced inline function has been removed
        '/wd4706', # assignment within conditional expression
        '/wd4710', # function not inlined
        '/wd4711', # function selected for automatic inline expansion
        '/wd4800', # implicit conversion from int to bool
        '/wd4820', # padding added after construct
        '/wd4996', # POSIX name for this item is deprecated
        '/wd5045', # compiler will insert Spectre mitigation
      ]
    endif
  endif

  c_suppressions = cc.get_supported_arguments(c_suppressions)
endif

#######
# C++ #
#######

if is_variable('cpp')
  cpp_suppressions = []

  if cpp.get_id() == 'clang'
    cpp_suppressions += clang_common_suppressions

    if warning_level == 'everything'
      cpp_suppressions += [
        '-Wno-c++98-compat',
        '-Wno-exit-time-destructors',
        '-Wno-global-constructors',
        '-Wno-missing-noreturn',
        '-Wno-old-style-cast',
        '-Wno-unsafe-buffer-usage',
        '-Wno-weak-vtables',
        '-Wno-zero-as-null-pointer-constant',
      ]
    endif

    if warning_level in ['everything', '3']
      cpp_suppressions += [
        '-Wno-cast-function-type',
      ]
    endif

  elif cpp.get_id() == 'gcc'
    cpp_suppressions += gcc_common_suppressions

    if warning_level == 'everything'
      cpp_suppressions += [
        '-Wno-effc++',
        '-Wno-missing-noreturn',
        '-Wno-null-dereference',
        '-Wno-old-style-cast',
        '-Wno-redundant-tags',
        '-Wno-useless-cast',
        '-Wno-zero-as-null-pointer-constant',
      ]
    endif

    if warning_level in ['everything', '3']
      cpp_suppressions += [
        '-Wno-pedantic',
      ]
    endif

    if warning_level in ['everything', '3', '2']
      cpp_suppressions += [
        '-Wno-cast-function-type',
      ]
    endif
  endif

  cpp_suppressions = cpp.get_supported_arguments(cpp_suppressions)
endif
