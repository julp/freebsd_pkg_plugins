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

    string(TOUPPER "${PKG_PLUGIN_NAME}" UPPER_CASED_PKG_PLUGIN_NAME)
    list(APPEND PKG_PLUGIN_DEFINITIONS "NAME=\"${PKG_PLUGIN_NAME}\"")
    add_library(${PKG_PLUGIN_NAME} SHARED ${PKG_PLUGIN_SOURCES})
    if(PKG_PLUGIN_LIBRARIES)
        target_link_libraries(${PKG_PLUGIN_NAME} ${PKG_PLUGIN_LIBRARIES} ${pkg_LIBRARY})
    endif(PKG_PLUGIN_LIBRARIES)

    if(PKG_PLUGIN_VERSION)
        _set_version("${PKG_PLUGIN_VERSION}" "${UPPER_CASED_PKG_PLUGIN_NAME}")
        list(APPEND PKG_PLUGIN_DEFINITIONS "${UPPER_CASED_PKG_PLUGIN_NAME}_VERSION_MAJOR=${${UPPER_CASED_PKG_PLUGIN_NAME}_VERSION_MAJOR}")
        list(APPEND PKG_PLUGIN_DEFINITIONS "${UPPER_CASED_PKG_PLUGIN_NAME}_VERSION_MINOR=${${UPPER_CASED_PKG_PLUGIN_NAME}_VERSION_MINOR}")
        list(APPEND PKG_PLUGIN_DEFINITIONS "${UPPER_CASED_PKG_PLUGIN_NAME}_VERSION_PATCH=${${UPPER_CASED_PKG_PLUGIN_NAME}_VERSION_PATCH}")
        list(APPEND PKG_PLUGIN_DEFINITIONS "${UPPER_CASED_PKG_PLUGIN_NAME}_VERSION_NUMBER=${${UPPER_CASED_PKG_PLUGIN_NAME}_VERSION_NUMBER}")
        list(APPEND PKG_PLUGIN_DEFINITIONS "${UPPER_CASED_PKG_PLUGIN_NAME}_VERSION_STRING=\"${${UPPER_CASED_PKG_PLUGIN_NAME}_VERSION_STRING}\"")
    endif(PKG_PLUGIN_VERSION)

    # <pkg 1.18>
    include(CheckLibraryExists)
    check_library_exists("${pkg_LIBRARY}" "pkg_object_find" "" HAVE_PKG_OBJECT_FIND)
    if(HAVE_PKG_OBJECT_FIND)
        list(APPEND PKG_PLUGIN_DEFINITIONS "HAVE_PKG_OBJECT_FIND=1")
    endif(HAVE_PKG_OBJECT_FIND)

    check_library_exists("${pkg_LIBRARY}" "pkg_shlibs_required" "" HAVE_PKG_SHLIBS_REQUIRED)
    if(HAVE_PKG_SHLIBS_REQUIRED)
        list(APPEND PKG_PLUGIN_DEFINITIONS "HAVE_PKG_SHLIBS_REQUIRED=1")
    endif(HAVE_PKG_SHLIBS_REQUIRED)
    # </pkg 1.18>

    # <pkg 1.20>
    # PKG_* constants were renamed to PKG_ATTR_* (commit d9c65f6). See: https://github.com/freebsd/pkg/commit/d9c65f6896cf264bcf9926d553c32d92ddfb1ffb
    include(CheckSourceCompiles)
    list(APPEND CMAKE_REQUIRED_INCLUDES "${pkg_INCLUDE_DIR}")
    check_source_compiles(C [=[
#include <stdlib.h>
#include <pkg.h>

int main(void)
{
    pkg_attr origin = PKG_ATTR_ORIGIN;

    return EXIT_SUCCESS;
}
    ]=] HAVE_PKG_ATTR)

    if(HAVE_PKG_ATTR)
        list(APPEND PKG_PLUGIN_DEFINITIONS "HAVE_PKG_ATTR=1")
    endif(HAVE_PKG_ATTR)
    # </pkg 1.20>

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
