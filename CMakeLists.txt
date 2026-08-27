# Simple demo writer that uses libpng3d APIs to write an example PNG.
# This CMake only for building the demos
cmake_minimum_required(VERSION 3.5)
project(libpng3d-demo C)

find_package(PNG REQUIRED)

include_directories(${PNG_INCLUDE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/include)

add_executable(demo_writer tools/demo_writer.c src/png3d.c)
target_link_libraries(demo_writer ${PNG_LIBRARY})

add_executable(demo_reader tools/demo_reader.c src/png3d.c)
target_link_libraries(demo_reader ${PNG_LIBRARY})
