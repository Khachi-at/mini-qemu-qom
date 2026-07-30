project(
    'mini-qemu-qom',
    'c',
    version: '0.1.0',
    default_options: ['c_std=c17', 'warning_level=3'],
)

inc = include_directories('include')

core_sources = files(
    'src/qom/type.c',
    'src/qom/object.c',
    'src/qom/property.c',
    'src/memory/hostmem.c',
    'src/memory/hostmem_memfd.c',
)

core = static_library('miniqom', core_sources, include_directories: inc)

executable(
    'mini-qom-demo',
    'src/main.c',
    include_directories: inc,
    link_with: core,
)

test_exe = executable(
    'test-qom',
    'tests/test_qom.c',
    include_directories: inc,
    link_with: core,
)

test('qom', test_exe)
