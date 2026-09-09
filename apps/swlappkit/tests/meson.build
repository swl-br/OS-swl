test_menubar_exe = executable('test_menubar',
  'test_menubar.c',
  include_directories: include_directories('../include'),
  dependencies: [cairo, pangocairo],
  link_with: swlappkit_lib,
)

test('menubar', test_menubar_exe)