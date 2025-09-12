libtargets = []
libtargets += static_library('libavx',
  files(
    'x86simdsort-avx2.cpp',
    ),
  include_directories : [src],
  cpp_args : cpp.get_id() == 'msvc' ? ['/arch:AVX2'] : ['-march=haswell'],
  gnu_symbol_visibility : 'inlineshidden',
  dependencies: [omp_dep],
  )

libtargets += static_library('libskx',
  files(
    'x86simdsort-skx.cpp',
    ),
  include_directories : [src],
  cpp_args : cpp.get_id() == 'msvc' ? ['/arch:AVX512'] : ['-march=skylake-avx512'],
  gnu_symbol_visibility : 'inlineshidden',
  dependencies: [omp_dep],
  )

libtargets += static_library('libicl',
  files(
    'x86simdsort-icl.cpp',
    ),
  include_directories : [src],
  cpp_args : cpp.get_id() == 'msvc' ? ['/arch:AVX512'] : ['-march=icelake-client'],
  gnu_symbol_visibility : 'inlineshidden',
  dependencies: [omp_dep],
  )

if cancompilefp16
  libtargets += static_library('libspr',
    files(
      'x86simdsort-spr.cpp',
      ),
    include_directories : [src],
    cpp_args : cpp.get_id() == 'msvc' ? ['/arch:AVX512'] : ['-march=sapphirerapids'],
    gnu_symbol_visibility : 'inlineshidden',
    dependencies: [omp_dep],
    )
endif

install_headers('x86simdsort.h')
