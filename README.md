# EGT Sample Project

This is a basic example of an application using Ensemble Graphics Toolkit (EGT) for embedded Linux systems such as the ATSAMA5D27-WLSOM1.

## Requirements

- CMake >= 3.10
- EGT (https://github.com/linux4sam/egt)
- A toolchain for cross-compiling to SAMA5 or native build support if running on a framebuffer-capable target

## Build

```bash
mkdir build && cd build
cmake ..
make
