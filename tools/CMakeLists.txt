cmake_minimum_required(VERSION 3.5)
project(libpng3d_demos C)

#find_package(PNG REQUIRED)
include_directories(${CMAKE_SOURCE_DIR})
add_executable(demo_writer demo_writer.c ../src/png3d.c)
#target_include_directories(demo_writer PRIVATE ${PNG_INCLUDE_DIRS} .)
target_link_libraries(demo_writer PRIVATE ${PNG_LIBRARIES} ${PNG_LIBRARY_TARGETS})

add_executable(demo_reader demo_reader.c ../src/png3d.c)
#target_include_directories(demo_reader PRIVATE ${PNG_INCLUDE_DIRS} .)
target_link_libraries(demo_reader PRIVATE ${PNG_LIBRARIES} ${PNG_LIBRARY_TARGETS})
