# mini-qemu-qom

A minimal QEMU Object Model (QOM) practice project written in C.

This repository is not trying to fully reimplement QEMU. The goal is to connect a few core QOM ideas with as little code as possible:

- type registration and inheritance
- object creation and destruction
- object trees and path resolution
- class property registration and string-based assignment
- a simplified `memory-backend-memfd` example object

The project currently builds successfully, runs a demo program, and passes its basic tests.

## Project Layout

```text
.
├── include/miniqom/
│   ├── error.h
│   ├── memory.h
│   ├── object.h
│   ├── property.h
│   └── type.h
├── src/
│   ├── main.c
│   ├── memory/
│   │   ├── hostmem.c
│   │   └── hostmem_memfd.c
│   └── qom/
│       ├── object.c
│       ├── property.c
│       └── type.c
├── tests/
│   └── test_qom.c
└── meson.build
```

## Core Features

### 1. Type System

The project includes a minimal but usable type registration system:

- `type_system_init()` initializes the base type
- `type_register_static()` registers a static type
- `type_get_by_name()` looks up a type by name
- `type_get_parent()` returns the parent type
- `type_get_info()` returns the type metadata
- `type_get_class()` returns the class object for a type

Type metadata is described by `TypeInfo`, including:

- type name
- parent type name
- instance size
- class initialization callback
- instance initialization callback
- instance finalization callback

### 2. Object System

The object layer provides these basic capabilities:

- `object_new()` creates an object from a type name
- `object_free()` recursively frees an object tree
- `object_is_instance_of()` checks whether an object belongs to a type
- `object_add_child()` creates a parent-child relationship
- `object_resolve_path()` resolves a path such as `/objects/mem0`

### 3. Property System

The property system converts string input into object field values:

- supports `bool`, `uint64_t`, and `string` property types
- allows registering property setters on a class
- applies parsing and assignment through `object_property_set_from_string()`

This follows the same broad idea as QEMU object configuration through properties, but in a much smaller and easier-to-read form.

### 4. Memory Backend Example

The current code implements two example types:

- `memory-backend`
- `memory-backend-memfd`

They demonstrate:

- type inheritance
- parent and child property composition
- default value initialization
- conditional property validation

For example:

- `size` is a `uint64_t`
- `mmoc` is a boolean property
- `swap-storage` can only be set when `mmoc=on`
- `seal` and `hugetlb` belong to `memory-backend-memfd`

## Build Requirements

You will need:

- a C17 compiler
- [Meson](https://mesonbuild.com/)
- [Ninja](https://ninja-build.org/)

## Build and Run

Configure the build directory the first time:

```bash
meson setup build
```

Compile:

```bash
meson compile -C build
```

Run the demo program:

```bash
./build/mini-qom-demo
```

Run the tests:

```bash
meson test -C build
```

## Example Output

The demo program creates a small object tree, sets several properties on `mem0`, and prints output like this:

```text
<root> (object)
  objects (object)
    mem0 (memory-backend-memfd)

mem0: size=1073741824, share=1, mmoc=1, seal=0
```

This means:

- the root object contains `objects`
- `objects` contains `mem0`
- the dynamic type of `mem0` is `memory-backend-memfd`
- `size` was set to `1G`
- `mmoc` was enabled
- `seal` was explicitly disabled

## What the Tests Cover

`tests/test_qom.c` currently verifies:

- `memory-backend-memfd` is an instance of the child type, the parent type, and `object`
- the path `/objects/mem0` resolves correctly
- setting `swap-storage` fails when `mmoc=off`
- setting `swap-storage` succeeds after enabling `mmoc`
- `"2G"` is parsed correctly into `uint64_t`

## What This Project Is Good For

This project is a good fit if you are learning about:

- the basic design of QEMU QOM
- lightweight object-oriented patterns in C
- how a type system, object tree, and property system work together
- how to reduce a large framework into a small, understandable subset

## Current Scope and Limits

This is intentionally a practice project, so it stays small. It does not currently cover:

- dynamic type unloading
- reference counting
- thread safety
- more advanced property accessors
- device and bus models
- a more complete error propagation model
- the full object lifecycle details used by real QEMU

These are all reasonable directions for future expansion.

## Possible Next Steps

If you want to make it closer to real QEMU, you could:

1. add object reference counting and weak-reference behavior
2. support property reads, not only writes
3. add more memory backends or device types
4. expand the unit tests more systematically
5. add traversal and pretty-print helper APIs for object trees
6. introduce clearer module boundaries and error handling conventions

## License

There is no explicit license file in the repository yet. If you plan to publish it, adding a `LICENSE` file would be a good next step.
