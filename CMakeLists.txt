cmake_minimum_required(VERSION 3.10)
project(egt-sample LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Requiere pkg-config para localizar EGT
find_package(PkgConfig REQUIRED)

# Busca la librería libegt
pkg_check_modules(EGT REQUIRED libegt)

# Incluye las rutas de cabeceras
include_directories(${EGT_INCLUDE_DIRS})
link_directories(${EGT_LIBRARY_DIRS})

# Define el ejecutable
add_executable(egt-sample src/main.cpp)

# Enlaza contra libegt y pthread si es necesario
target_link_libraries(egt-sample ${EGT_LIBRARIES} pthread)

# Añade flags adicionales si EGT lo requiere
target_compile_options(egt-sample PRIVATE ${EGT_CFLAGS_OTHER})
