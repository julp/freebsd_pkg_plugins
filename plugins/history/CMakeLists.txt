cmake_minimum_required(VERSION 2.8.3...3.20.5)

find_package(RE2C 2 QUIET)

if(RE2C_FOUND)
    RE2C_TARGET(NAME "re2c_date_grammar" INPUT "${CMAKE_CURRENT_SOURCE_DIR}/date.re" OUTPUT "${CMAKE_BINARY_DIR}/date_scanner.gen.c" OPTIONS "-d8")
endif(RE2C_FOUND)

set(
    HISTORY_INCLUDE_DIRECTORIES
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/error
    ${PROJECT_SOURCE_DIR}/kissc
    ${PROJECT_SOURCE_DIR}/sqlite
)

set(
    HISTORY_LIBRARIES
    kvm
    $<TARGET_OBJECTS:error>
    sqlite
    #$<TARGET_OBJECTS:sqlite>
    #${SQLite3_LIBRARY}
)

set(COMMON_SOURCES
    #${CMAKE_BINARY_DIR}/date_scanner.gen.c
        ${PROJECT_SOURCE_DIR}/kissc/ascii_case.c
    ${PROJECT_SOURCE_DIR}/kissc/dlist.c
    ${PROJECT_SOURCE_DIR}/kissc/iterator.c
    ${PROJECT_SOURCE_DIR}/kissc/parsenum.c
)

if(RE2C_FOUND)
    list(APPEND COMMON_SOURCES ${CMAKE_BINARY_DIR}/date_scanner.gen.c)
else(RE2C_FOUND)
    list(APPEND COMMON_SOURCES date_scanner.gen.c)
endif(RE2C_FOUND)

set(
    HISTORY_SOURCES
    ${COMMON_SOURCES}
    plugin_history.c
    ${PROJECT_SOURCE_DIR}/shared/os.c
    ${PROJECT_SOURCE_DIR}/shared/compat.c
    ${PROJECT_SOURCE_DIR}/shared/path_join.c
    ${PROJECT_SOURCE_DIR}/shared/argv.c
    ${PROJECT_SOURCE_DIR}/kissc/stpcpy_sp.c
)

pkg_plugin(
    INSTALL
    NAME history
    VERSION "0.8.0"
    SOURCES ${HISTORY_SOURCES}
    LIBRARIES ${HISTORY_LIBRARIES}
    INCLUDE_DIRECTORIES ${HISTORY_INCLUDE_DIRECTORIES}
)

add_executable(test_date
    ${COMMON_SOURCES}
    test_date.c
)
set_target_properties(test_date PROPERTIES
    INCLUDE_DIRECTORIES "${HISTORY_INCLUDE_DIRECTORIES};${PROJECT_SOURCE_DIR};${PROJECT_BINARY_DIR}"
)
target_link_libraries(test_date $<TARGET_OBJECTS:error>)
