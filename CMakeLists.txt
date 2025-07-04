cmake_minimum_required(VERSION 3.10)
project(egt-sample LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(EGT REQUIRED libegt)

include_directories(${EGT_INCLUDE_DIRS})
link_directories(${EGT_LIBRARY_DIRS})

file(GLOB SRC
    "src/*.cpp"
    "src/screens/*.cpp"
)

file(GLOB HEADERS
    "src/*.h"
    "src/screens/*.h"
)

add_executable(egt-sample ${SRC})

target_link_libraries(egt-sample ${EGT_LIBRARIES} pthread)
target_compile_options(egt-sample PRIVATE ${EGT_CFLAGS_OTHER})