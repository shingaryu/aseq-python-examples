project('spectrometer', 'c')
add_project_arguments('-Dlibspectrometer_EXPORTS', language : 'c')

install_headers('include/libspectrometer.h', subdir : 'libspectrometer')

if host_machine.system() == 'windows'
  cc = meson.get_compiler('c')
  # TODO Find an alternative way to specify library directory
  hidapi = cc.find_library('hidapi', dirs : join_paths(meson.current_source_dir(), 'lib'))
else
  hidapi = dependency('hidapi-hidraw')
  if not hidapi.found()
    hidapi = dependency('hidapi-libusb')
  endif
endif
lib = shared_library('spectrometer', ['src/internal.c', 'src/libspectrometer.c'],
                     include_directories : include_directories('include'),
                     dependencies : hidapi,
                     install : true,
                     soversion : 1)

# TODO What about Windows machines with pkg-config installed?
if host_machine.system() != 'windows'
  pkg = import('pkgconfig')
  pkg.generate(libraries : lib,
               name : 'libspectrometer',
               subdirs: 'libspectrometer',
               requires_private: hidapi,
               description : 'Library for ASEQ Instrument\'s spectrometers')
endif
