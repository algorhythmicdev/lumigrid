cmake_minimum_required(VERSION 3.16)

set(CMAKE_NINJA_FORCE_RESPONSE_FILE ON CACHE BOOL "Force response files for large link commands" FORCE)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(EXTRA_COMPONENT_DIRS components)

project(lumigrid_lednode)
