cmake_minimum_required(VERSION 3.10)
project(egt-sample LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Buscar libegt
find_package(PkgConfig REQUIRED)
pkg_check_modules(EGT REQUIRED libegt)

# Incluir y enlazar con EGT
include_directories(${EGT_INCLUDE_DIRS})
link_directories(${EGT_LIBRARY_DIRS})

# Archivos fuente y headers
file(GLOB SRC
    "src/*.cpp"
    "src/screens/*.cpp"
)

file(GLOB HEADERS
    "src/*.h"
    "src/screens/*.h"
)

# Crear ejecutable
add_executable(egt-app ${SRC})

# Enlazar bibliotecas
target_link_libraries(egt-app ${EGT_LIBRARIES} pthread)
target_compile_options(egt-app PRIVATE ${EGT_CFLAGS_OTHER})

# Instalar binario en /usr/bin (o ${CMAKE_INSTALL_BINDIR})
include(GNUInstallDirs)
install(TARGETS egt-app
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
