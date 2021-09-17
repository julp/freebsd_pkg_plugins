include(CommonFind)

common_find_package(
    NAME pkg
    LIBRARIES pkg
    HEADER pkg.h
    PKG_CONFIG_MODULE_NAME pkg
    VERSION_REGEXPES "# *define +PKGVERSION *\"([0-9.]+)\""
)

find_program(pkg_EXECUTABLE pkg)
if(pkg_EXECUTABLE)
    execute_process(OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND ${pkg_EXECUTABLE} config pkg_plugins_dir OUTPUT_VARIABLE pkg_PLUGINS_DIR)
endif(pkg_EXECUTABLE)

function(_set_version VERSION_STRING OUTPUT_PREFIX)
    string(REGEX MATCHALL "[0-9]+" VERSION_PARTS ${VERSION_STRING})
    list(GET VERSION_PARTS 0 VERSION_MAJOR)
    list(GET VERSION_PARTS 1 VERSION_MINOR)
    list(GET VERSION_PARTS 2 VERSION_PATCH)

    math(EXPR VERSION_NUMBER "${VERSION_MAJOR} * 1000 + ${VERSION_MINOR} * 100 + ${VERSION_PATCH}")

    set(${OUTPUT_PREFIX}_VERSION_MAJOR ${VERSION_MAJOR} PARENT_SCOPE)
    set(${OUTPUT_PREFIX}_VERSION_MINOR ${VERSION_MINOR} PARENT_SCOPE)
    set(${OUTPUT_PREFIX}_VERSION_PATCH ${VERSION_PATCH} PARENT_SCOPE)
    set(${OUTPUT_PREFIX}_VERSION_STRING ${VERSION_STRING} PARENT_SCOPE)
    set(${OUTPUT_PREFIX}_VERSION_NUMBER ${VERSION_NUMBER} PARENT_SCOPE)
endfunction(_set_version)

macro(pkg_plugin)
    cmake_parse_arguments(
        PKG_PLUGIN # output variable name
        # options (true/false) (default value: false)
        "INSTALL"
        # univalued parameters (default value: "")
        "NAME;VERSION"
        # multivalued parameters (default value: "")
        "INCLUDE_DIRECTORIES;LIBRARIES;SOURCES;DEFINITIONS"
        ${ARGN}
    )

    if(NOT PKG_PLUGIN_NAME)
        message(FATAL_ERROR "pkg_plugin, missing expected argument: NAME")
    endif(NOT PKG_PLUGIN_NAME)
    if(NOT PKG_PLUGIN_SOURCES)
        message(FATAL_ERROR "pkg_plugin, missing expected argument: SOURCES")
    endif(NOT PKG_PLUGIN_SOURCES)

    add_library(${PKG_PLUGIN_NAME} SHARED ${PKG_PLUGIN_SOURCES})
    if(PKG_PLUGIN_LIBRARIES)
        target_link_libraries(${PKG_PLUGIN_NAME} ${PKG_PLUGIN_LIBRARIES} ${pkg_LIBRARY})
    endif(PKG_PLUGIN_LIBRARIES)

    if(PKG_PLUGIN_VERSION)
        _set_version("${PKG_PLUGIN_VERSION}" "${PKG_PLUGIN_NAME}")
    endif(PKG_PLUGIN_VERSION)

    # TODO: version ?
    set(PKG_PLUGIN_INCLUDE_DIRS )
    list(APPEND PKG_PLUGIN_INCLUDE_DIRS ${PROJECT_SOURCE_DIR})
    list(APPEND PKG_PLUGIN_INCLUDE_DIRS ${PROJECT_BINARY_DIR})
    list(APPEND PKG_PLUGIN_INCLUDE_DIRS ${pkg_INCLUDE_DIR})
    list(APPEND PKG_PLUGIN_INCLUDE_DIRS ${PKG_PLUGIN_INCLUDE_DIRECTORIES})

    set_target_properties(${PKG_PLUGIN_NAME} PROPERTIES
        PREFIX ""
        COMPILE_DEFINITIONS "${PKG_PLUGIN_DEFINITIONS}"
        INCLUDE_DIRECTORIES "${PKG_PLUGIN_INCLUDE_DIRS}"
    )

    if(PKG_PLUGIN_INSTALL)
        install(TARGETS ${PKG_PLUGIN_NAME} DESTINATION ${pkg_PLUGINS_DIR})
    endif(PKG_PLUGIN_INSTALL)
endmacro(pkg_plugin)
