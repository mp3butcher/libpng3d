cmake_minimum_required(VERSION 3.5)
project(libpng3d_demos C)

find_package(PNG REQUIRED)

add_executable(demo_writer tools/demo_writer.c src/png3d.c)
target_include_directories(demo_writer PRIVATE ${PNG_INCLUDE_DIRS} .)
target_link_libraries(demo_writer PRIVATE ${PNG_LIBRARIES})

add_executable(demo_reader tools/demo_reader.c src/png3d.c)
target_include_directories(demo_reader PRIVATE ${PNG_INCLUDE_DIRS} .)
target_link_libraries(demo_reader PRIVATE ${PNG_LIBRARIES})
