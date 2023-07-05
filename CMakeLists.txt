################################################################################
cmake_minimum_required(VERSION 3.3)
project(geotools)
################################################################################

list(PREPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Libigl
include(libigl)

add_subdirectory(convert_mesh)
add_subdirectory(normalize_mesh)
add_subdirectory(poisson_disk)
add_subdirectory(voxmesh)
