cmake_minimum_required(VERSION 2.8.3)

find_package(SQLite3 REQUIRED)
#debug("SQLite3")

add_library(sqlite
    #STATIC
    OBJECT
    #EXCLUDE_FROM_ALL
    sqlite.c
    #${PROJECT_SOURCE_DIR}/kissc/iterator.c
    #${PROJECT_SOURCE_DIR}/shared/os.c
    #${PROJECT_SOURCE_DIR}/kissc/ascii_case.c
)
target_link_libraries(sqlite #[[PRIVATE $<TARGET_OBJECTS:error>]] PUBLIC ${SQLite3_LIBRARY})
#target_link_libraries(sqlite $<TARGET_OBJECTS:error> ${SQLite3_LIBRARY})

#get_target_property(INHERITED_INCLUDE_DIRECTORIES sqlite INCLUDE_DIRECTORIES)
set(SQLITE_MODULE_INCLUDE_DIRECTORIES
    ${SQLite3_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/error
    ${PROJECT_SOURCE_DIR}/kissc
)

set_target_properties(sqlite PROPERTIES
    COMPILE_FLAGS "-fPIC"
    INCLUDE_DIRECTORIES "${SQLITE_MODULE_INCLUDE_DIRECTORIES}"
)
