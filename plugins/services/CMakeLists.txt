cmake_minimum_required(VERSION 2.8.3...3.20.5)

set(
    SERVICES_INCLUDE_DIRECTORIES
    ${PROJECT_SOURCE_DIR}/error
    ${PROJECT_SOURCE_DIR}/kissc
    #${PROJECT_SOURCE_DIR}/shared
)

set(
    SERVICES_LIBRARIES
    util # fparseln
    $<TARGET_OBJECTS:error>
)

set(
    SERVICES_SOURCES
    rcorder.c
    services_db.c
    services_result.c
    services_selection.c
    plugin_services.c
    process_monitor.c
    ${PROJECT_SOURCE_DIR}/shared/os.c
    ${PROJECT_SOURCE_DIR}/shared/compat.c
    ${PROJECT_SOURCE_DIR}/shared/path_join.c
    ${PROJECT_SOURCE_DIR}/shared/argv.c
    ${PROJECT_SOURCE_DIR}/kissc/dlist.c
    ${PROJECT_SOURCE_DIR}/kissc/stpcpy_sp.c
    ${PROJECT_SOURCE_DIR}/kissc/hashtable.c
        ${PROJECT_SOURCE_DIR}/kissc/iterator.c
        ${PROJECT_SOURCE_DIR}/kissc/ascii_case.c
)

pkg_plugin(
    INSTALL
    NAME services
    VERSION "0.6.1"
    SOURCES ${SERVICES_SOURCES}
    LIBRARIES ${SERVICES_LIBRARIES}
    INCLUDE_DIRECTORIES ${SERVICES_INCLUDE_DIRECTORIES}
)
