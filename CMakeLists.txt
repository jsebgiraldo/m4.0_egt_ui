cmake_minimum_required(VERSION 3.10)
project(egt-sample LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Buscar libegt
find_package(PkgConfig REQUIRED)
pkg_check_modules(EGT REQUIRED libegt)

# Buscar D-Bus
pkg_check_modules(DBUS REQUIRED dbus-1)

# Incluir directorios
include_directories(
    ${EGT_INCLUDE_DIRS}
    ${DBUS_INCLUDE_DIRS}
)

# Directorios de bibliotecas
link_directories(
    ${EGT_LIBRARY_DIRS}
    ${DBUS_LIBRARY_DIRS}
)

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
add_executable(egt-sample ${SRC})

# Enlazar bibliotecas
target_link_libraries(egt-sample
    ${EGT_LIBRARIES}
    ${DBUS_LIBRARIES}
    pthread
)

# Compilar con flags adicionales
target_compile_options(egt-sample PRIVATE
    ${EGT_CFLAGS_OTHER}
    ${DBUS_CFLAGS_OTHER}
)

# Instalar binario
include(GNUInstallDirs)
install(TARGETS egt-sample
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
