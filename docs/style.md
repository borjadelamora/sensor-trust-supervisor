# Style

How the code in this repo is written. It follows the
[ROS 2 code style](https://docs.ros.org/en/rolling/The-ROS2-Project/Contributing/Code-Style-Language-Versions.html)
and the [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines);
what is below is where I have made a specific choice worth writing down. It grows as the
project does.

## Toolchain

Ubuntu 22.04, ROS 2 Humble, colcon and ament_cmake. C++23, so GCC 12 or newer: `std::expected`
is not in GCC 11. Python 3.10.

## Warnings

Every C++ target builds with:

```
-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wnon-virtual-dtor -Werror
```

Fix the warning rather than silencing it. If a suppression is genuinely the right answer it
gets a comment saying why.

## Formatting and lint

`.clang-format` and `.clang-tidy` at the repo root settle this. 100 columns, 2-space indent.
Format on save is configured. Before committing:

```
clang-tidy -p build/<package> <file>
```

Warnings are errors there too. Two checks are switched off and it is worth knowing why.
`readability-function-cognitive-complexity` ends up scoring the expansion of the rclcpp logging
macros instead of my code, and scored a five-line callback at 52.
`modernize-use-trailing-return-type` is a house preference I don't share.

## Naming

Packages are `snake_case` and prefixed `quorum_`. Types are `CamelCase`, functions and
variables `snake_case`, data members get a `trailing_underscore_`, compile-time constants are
`kCamelCase`. Files are `snake_case`, headers `.hpp` and sources `.cpp`, one public class per
header named after it. Topics, services and parameters are `snake_case`.

## Units and frames in names

Every physical quantity carries its unit in the name: `altitude_m`, `speed_mps`, `yaw_rad`,
`dt_s`.

Frames get the same treatment. PX4 works in NED and ROS works in ENU, so anything
frame-dependent says which frame it is in, and conversions happen in one named place rather
than inline wherever they are needed.

## C++ habits

`const` by default on locals, parameters and methods. `auto` only where the type is obvious
from the right-hand side. No naked `new` or `delete`: `std::unique_ptr` owns things, and
`shared_ptr` stays at the rclcpp boundary rather than spreading into ordinary logic.

Anything bigger than a couple of words gets passed by reference rather than copied, and by
`const` reference if the callee only reads it. A raw pointer is reserved for the case where the
thing can legitimately be absent. Neither a reference nor a raw pointer owns what it refers to.

## Python

`ruff` for both lint and format, same 100 columns. Type hints on public functions. Every random
draw goes through a seeded generator passed down from the scenario rather than a module-level
`random` call, because reproducible runs are one of the things this project is meant to
demonstrate.
