cmake_minimum_required(VERSION 2.8.3...3.20.5)

set(
    VERBOSE_INCLUDE_DIRECTORIES
    ${PROJECT_SOURCE_DIR}/error
    ${PROJECT_SOURCE_DIR}/kissc
)

set(
    VERBOSE_LIBRARIES
    $<TARGET_OBJECTS:error>
)

set(
    VERBOSE_SOURCES
    plugin_verbose.c
)

pkg_plugin(
    INSTALL
    NAME verbose
    SOURCES ${VERBOSE_SOURCES}
    LIBRARIES ${VERBOSE_LIBRARIES}
    INCLUDE_DIRECTORIES ${VERBOSE_INCLUDE_DIRECTORIES}
)
