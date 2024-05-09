project('AudioPlayer', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++23'])

gtkdep = [
  dependency('gtk4'), 
  dependency('libadwaita-1'),
  dependency('taglib')
  ]

executable('AudioPlayer',
          'src/main.cpp',
          install : true,
          dependencies : gtkdep)
