byzanz_gui_sources = [
  'main.c',
  'byzanz-gui-window.c',
]

byzanz_gui_deps = [
  dependency('gio-2.0', version: '>= 2.50'),
  dependency('gtk+-3.0', version: '>= 3.22'),
]

gnome = import('gnome')

byzanz_gui_sources += gnome.compile_resources('byzanz-gui-resources',
  'byzanz-gui.gresource.xml',
  c_name: 'byzanz_gui'
)

executable('byzanz-gui', byzanz_gui_sources,
  dependencies: byzanz_gui_deps,
  install: true,
)
