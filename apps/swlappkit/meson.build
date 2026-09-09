project('swlappkit', 'c',
  version: '0.1.0',
  default_options: [
    'c_std=c11',
    'warning_level=2',
    'buildtype=release',
  ],
)

cairo      = dependency('cairo')
pangocairo = dependency('pangocairo')

swlappkit_lib = static_library('swlappkit',
  'src/menubar.c',
  include_directories: include_directories('include'),
  dependencies: [cairo, pangocairo],
  install: false,
)

swlappkit_dep = declare_dependency(
  link_with: swlappkit_lib,
  include_directories: include_directories('include'),
  dependencies: [cairo, pangocairo],
)

subdir('tests')