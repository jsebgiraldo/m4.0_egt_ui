cmake_minimum_required(VERSION 3.10)
project(egt-app LANGUAGES CXX)

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
    "src/wifi/*.cpp"
)

file(GLOB HEADERS
    "src/*.h"
    "src/screens/*.h"
    "src/wifi/*.h"
)

# Crear ejecutable
add_executable(egt-app ${SRC})

# Enlazar bibliotecas
target_link_libraries(egt-app
    ${EGT_LIBRARIES}
    ${DBUS_LIBRARIES}
    pthread
)

# Compilar con flags adicionales
target_compile_options(egt-app PRIVATE
    ${EGT_CFLAGS_OTHER}
    ${DBUS_CFLAGS_OTHER}
)

# AGREGAR: Copiar archivos de assets al directorio de build
file(GLOB ASSETS_FILES
    "assets/image/*.png"
    "assets/image/*.jpg"
    "assets/image/*.jpeg"
)

# Crear directorio de assets en build
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/assets/image)

# Copiar assets al directorio de build
foreach(ASSET_FILE ${ASSETS_FILES})
    get_filename_component(ASSET_NAME ${ASSET_FILE} NAME)
    configure_file(${ASSET_FILE} ${CMAKE_BINARY_DIR}/assets/image/${ASSET_NAME} COPYONLY)
endforeach()

# ALTERNATIVA: Usar add_custom_command para copiar automáticamente
add_custom_target(copy_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets
    ${CMAKE_BINARY_DIR}/assets
    COMMENT "Copying assets to build directory"
)

# Hacer que egt-app dependa de copy_assets
add_dependencies(egt-app copy_assets)

# Instalar binario
include(GNUInstallDirs)
install(TARGETS egt-app
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Instalar assets
install(DIRECTORY assets/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/egt-app/assets
        FILES_MATCHING PATTERN "*.png" PATTERN "*.jpg" PATTERN "*.jpeg")
