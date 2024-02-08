project('ffitest', 'cpp', default_options: ['cpp_std=c++23'])

cppc = meson.get_compiler('cpp')

rslib = declare_dependency(
    dependencies: cppc.find_library('mylibffi', dirs: [meson.current_source_dir() + '/../../mylibffi/target/debug']),
    include_directories : include_directories('include')
)

executable('ffitest', ['main.cpp', 'mylib.cpp', 'Runtime.cpp'] , dependencies: rslib)

