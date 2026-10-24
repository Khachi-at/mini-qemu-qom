project(
    'mini-qemu-qom',
    'c',
    version: '0.1.0',
    default_options: ['c_std=c17', 'warning_level=3'],
)

inc = include_directories('include')

core_sources = files(
    'src/device/register_device.c',
    'src/memory/address_space.c',
    'src/memory/hostmem.c',
    'src/memory/hostmem_memfd.c',
    'src/memory/memory_region.c',
    'src/qom/object.c',
    'src/qom/property.c',
    'src/qom/type.c',
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

device_test_exe = executable(
    'test-register-device',
    'tests/test_register_device.c',
    include_directories: inc,
    link_with: core,
)

foreach case_name : [
    'lifecycle',
    'read-only-status',
    'configuration-validation',
    'reconfiguration',
    'zero-base-configuration-freeze',
    'mapping-conflict',
]
    test('register-device-' + case_name, device_test_exe, args: [case_name])
endforeach
