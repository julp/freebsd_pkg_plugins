include(CommonFind)

common_find_package(
    NAME SQLite3
    LIBRARIES sqlite3
    HEADER sqlite3.h
    PKG_CONFIG_MODULE_NAME sqlite3
#     OUTPUT_VARIABLE_NAME "SQLITE3"
#     VERSION_REGEXPES "# *define *SQLITE_VERSION_NUMBER *([0-9]+)"
    VERSION_REGEXPES "# *define *SQLITE_VERSION *\"([0-9.]+)\""
)
