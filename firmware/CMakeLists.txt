cmake_minimum_required(VERSION 3.16)

set(CMAKE_NINJA_FORCE_RESPONSE_FILE ON CACHE BOOL "Force response files for large link commands" FORCE)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(EXTRA_COMPONENT_DIRS components)

idf_build_set_property(COMPILE_OPTIONS "-Wno-format-truncation" APPEND)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/main/tests")
  unity_add_basic_test(NAME fx_crc TEST_SRC "main/tests/test_fx_crc.c")
  unity_add_basic_test(NAME pwm_groups TEST_SRC "main/tests/test_pwm_groups.c")
  unity_add_basic_test(NAME config_merge TEST_SRC "main/tests/test_config_merge.c")
endif()

project(lumigrid_lednode REQUIRES unity)
