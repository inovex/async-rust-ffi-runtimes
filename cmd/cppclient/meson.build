project('ffitest', 'cpp', default_options: ['cpp_std=c++23'])

cppc = meson.get_compiler('cpp')

boost = dependency('boost', version : '>=1.74.0')
openssl = dependency('openssl', method : 'system')
rslib = declare_dependency(
    dependencies: cppc.find_library('mylibffi', dirs: [meson.current_source_dir() + '/../../mylibffi/target/debug']),
    include_directories : include_directories('include')
)

executable('ffitest', ['main.cpp', 'http.cpp', 'mylib.cpp', 'Runtime.cpp'] , dependencies: [boost, openssl, rslib])

