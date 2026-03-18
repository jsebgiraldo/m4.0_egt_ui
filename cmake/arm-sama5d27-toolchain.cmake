# CMake Toolchain File for SAMA5D27 (ARM Cortex-A5)
# Cross-compilation using Yocto SDK

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Yocto SDK paths (set by environment-setup-* or manually)
if(DEFINED ENV{SDKTARGETSYSROOT})
    set(CMAKE_SYSROOT $ENV{SDKTARGETSYSROOT})
else()
    set(CMAKE_SYSROOT "/opt/yocto/tmp/work/sama5d27_wlsom1_ek_sd-poky-linux-gnueabi/suntek-image/1.0/rootfs" CACHE PATH "Yocto sysroot path")
endif()

# Cross-compiler
if(DEFINED ENV{CC})
    separate_arguments(_CC UNIX_COMMAND $ENV{CC})
    list(GET _CC 0 CMAKE_C_COMPILER)
else()
    set(CMAKE_C_COMPILER arm-poky-linux-gnueabi-gcc CACHE FILEPATH "C compiler")
endif()

if(DEFINED ENV{CXX})
    separate_arguments(_CXX UNIX_COMMAND $ENV{CXX})
    list(GET _CXX 0 CMAKE_CXX_COMPILER)
else()
    set(CMAKE_CXX_COMPILER arm-poky-linux-gnueabi-g++ CACHE FILEPATH "C++ compiler")
endif()

# Search paths
set(CMAKE_FIND_ROOT_PATH ${CMAKE_SYSROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# pkg-config sysroot
set(ENV{PKG_CONFIG_SYSROOT_DIR} ${CMAKE_SYSROOT})
set(ENV{PKG_CONFIG_PATH} "${CMAKE_SYSROOT}/usr/lib/pkgconfig:${CMAKE_SYSROOT}/usr/share/pkgconfig")
