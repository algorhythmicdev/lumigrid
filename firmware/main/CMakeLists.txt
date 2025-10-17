set(COMPONENT_SRCS
    "app_main.c")

if(CONFIG_LUMIGRID_ENABLE_INTERNAL_TESTS)
    list(APPEND COMPONENT_SRCS "tests/test_pca9685_driver.c")
endif()

set(COMPONENT_INCLUDE_DIRS
    "."
    "config")

set(component_requires pca9685_driver)
if(CONFIG_LUMIGRID_ENABLE_INTERNAL_TESTS)
    list(APPEND component_requires unity)
endif()

idf_component_register(SRCS ${COMPONENT_SRCS}
                      INCLUDE_DIRS ${COMPONENT_INCLUDE_DIRS}
                      REQUIRES ${component_requires})
