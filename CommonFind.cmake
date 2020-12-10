# This module provides a function to find common libraries
#
# Requirements:
# - CMake >= 2.8.3 (for new version of find_package_handle_standard_args)
#
# Prototype:
#   common_find_package([LIBRARIES <list of names>] [HEADER <name>] [SUFFIXES <list of suffixes>]
#                       [PKG_CONFIG_MODULE_NAME <name>]
#                       [VERSION_HEADER <name>] [VERSION_REGEXPES <list of regexpes>]
#                       [CONFIG_EXECUTABLE <name>] [CONFIG_EXECUTABLE_LIBRARY_ARGUMENTS <list of arguments>] [CONFIG_EXECUTABLE_INCLUDE_ARGUMENTS <list of arguments>])
# Arguments:
#   - PKG_CONFIG_MODULE_NAME: name of pkg-config to check (if provided)
#   - CONFIG_EXECUTABLE: name of an external executable from which informations can be extracted
#   - CONFIG_EXECUTABLE_LIBRARY_ARGUMENTS (default: --libs): list of arguments to pass to ${CONFIG_EXECUTABLE} to get library path
#   - CONFIG_EXECUTABLE_INCLUDE_ARGUMENTS (default: --includedir): list of arguments to pass to ${CONFIG_EXECUTABLE} to get include directory
#   - LIBRARIES (mandatory): name(s) of the library to find
#   - HEADER (mandatory): name of a major header to find
#   - SUFFIXES: directory names to pass as PATH_SUFFIXES to find_path and find_library
#   - VERSION_HEADER (default: same as HEADER): the name of header file from which to extract version informations
#   - VERSION_REGEXPES: one or more regexp to extract version number(s) from header file. Notes:
#       + use a single regexp for a dotted version (major.minor.patch) or a numeric version number (eg: 50630)
#       + provide 2 to 4 regexpes if major/minor(/patch/tweak) are on seperated lines/macros
#


#=============================================================================
# Copyright (c) 2016, julp
#
# Distributed under the OSI-approved BSD License
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#=============================================================================

cmake_minimum_required(VERSION 2.8.3)

function(common_find_package)

    set("__FUNCTION__" "common_find_package")

    include(CMakeParseArguments)
    cmake_parse_arguments(
        PARSED_ARGS # output variable name
        # options (true/false) (default value: false)
        ""
        # univalued parameters (default value: "")
        "PKG_CONFIG_MODULE_NAME;HEADER;CONFIG_EXECUTABLE;VERSION_HEADER"
        # multivalued parameters (default value: "")
        "LIBRARIES;SUFFIXES;CONFIG_EXECUTABLE_LIBRARY_ARGUMENTS;CONFIG_EXECUTABLE_INCLUDE_ARGUMENTS;VERSION_REGEXPES"
        ${ARGN}
    )

    # TODO:
    # - replace NAME and OUTPUT_VARIABLE_NAME variables?

    # <test/temporary>
    if(DEFINED CMAKE_FIND_PACKAGE_NAME) # CMake >= 2.8.10
        set(PARSED_ARGS_OUTPUT_VARIABLE_NAME ${CMAKE_FIND_PACKAGE_NAME})
    else(DEFINED CMAKE_FIND_PACKAGE_NAME)
        get_directory_property(LISTFILE_STACK LISTFILE_STACK)
        list(GET LISTFILE_STACK -2 FIND_MODULE_FILENAME)
        get_filename_component(FIND_MODULE_NAME_WE ${FIND_MODULE_FILENAME} NAME_WE)
        string(REGEX REPLACE "^[fF][iI][nN][dD]" "" FIND_MODULE_STRIPPED_NAME "${FIND_MODULE_NAME_WE}")
        set(PARSED_ARGS_OUTPUT_VARIABLE_NAME "${FIND_MODULE_STRIPPED_NAME}")
    endif(DEFINED CMAKE_FIND_PACKAGE_NAME)
    set(PARSED_ARGS_NAME ${PARSED_ARGS_OUTPUT_VARIABLE_NAME})
    # </test/temporary>

    # <argument validation>
    set(MANDATORY_ARGUMENTS "LIBRARIES" "HEADER")
    foreach(ARGUMENT ${MANDATORY_ARGUMENTS})
        if(NOT PARSED_ARGS_${ARGUMENT})
            message(FATAL_ERROR "${__FUNCTION__}: argument ${ARGUMENT} is not set")
        endif(NOT PARSED_ARGS_${ARGUMENT})
    endforeach(ARGUMENT)
    # </argument validation>

    set(PC_VAR_NS "PC_${PARSED_ARGS_OUTPUT_VARIABLE_NAME}")

    # <force default value for some arguments>
    if(NOT PARSED_ARGS_VERSION_HEADER)
        set(PARSED_ARGS_VERSION_HEADER "${PARSED_ARGS_HEADER}")
    endif(NOT PARSED_ARGS_VERSION_HEADER)
    if(NOT CONFIG_EXECUTABLE_INCLUDE_ARGUMENTS)
        set(CONFIG_EXECUTABLE_INCLUDE_ARGUMENTS "--includedir")
    endif(NOT CONFIG_EXECUTABLE_INCLUDE_ARGUMENTS)
    if(NOT PARSED_ARGS_CONFIG_EXECUTABLE_LIBRARY_ARGUMENTS)
        set(PARSED_ARGS_CONFIG_EXECUTABLE_LIBRARY_ARGUMENTS "--libs")
    endif(NOT PARSED_ARGS_CONFIG_EXECUTABLE_LIBRARY_ARGUMENTS)
    # </force default value for some arguments>

    # <retrieve informations from pkg-config if requested and possible>
    if(PARSED_ARGS_PKG_CONFIG_MODULE_NAME)
        find_package(PkgConfig QUIET)

        if(PKG_CONFIG_FOUND)
            pkg_check_modules(${PC_VAR_NS} ${PARSED_ARGS_PKG_CONFIG_MODULE_NAME} QUIET)
            if(${PC_VAR_NS}_FOUND)
                if(${PC_VAR_NS}_VERSION)
                    set(${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION "${${PC_VAR_NS}_VERSION}")
                    string(REGEX MATCHALL "[0-9]+" VERSION_PARTS "${${PC_VAR_NS}_VERSION}")
                    if(VERSION_PARTS)
                        list(LENGTH VERSION_PARTS VERSION_PARTS_LENGTH)
                        list(GET VERSION_PARTS 0 ${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION_MAJOR)
                        if(VERSION_PARTS_LENGTH GREATER 1)
                            list(GET VERSION_PARTS 1 ${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION_MINOR)
                        endif(VERSION_PARTS_LENGTH GREATER 1)
                        if(VERSION_PARTS_LENGTH GREATER 2)
                            list(GET VERSION_PARTS 2 ${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION_PATCH)
                        endif(VERSION_PARTS_LENGTH GREATER 2)
                        if(VERSION_PARTS_LENGTH GREATER 3)
                            list(GET VERSION_PARTS 3 ${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION_TWEAK)
                        endif(VERSION_PARTS_LENGTH GREATER 3)
                    endif(VERSION_PARTS)
                endif(${PC_VAR_NS}_VERSION)
            endif(${PC_VAR_NS}_FOUND)
        endif(PKG_CONFIG_FOUND)
    endif(PARSED_ARGS_PKG_CONFIG_MODULE_NAME)
    # </retrieve informations from pkg-config if requested and possible>

    # <same if an external tool is provided and found> (like mysql_config, pg_config, pcre-config, icu-config, ...)
    if(PARSED_ARGS_CONFIG_EXECUTABLE)
        find_program(CONFIG_EXECUTABLE ${PARSED_ARGS_CONFIG_EXECUTABLE})
        if(CONFIG_EXECUTABLE)
            execute_process(OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND ${CONFIG_EXECUTABLE} ${CONFIG_EXECUTABLE_LIBRARY_ARGUMENTS} OUTPUT_VARIABLE CONFIG_EXECUTABLE_LIBRARY)
            execute_process(OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND ${CONFIG_EXECUTABLE} ${CONFIG_EXECUTABLE_INCLUDE_ARGUMENTS} OUTPUT_VARIABLE CONFIG_EXECUTABLE_INCLUDE_DIR)
            # TODO: strip -I/-L in the above outputs?
        endif(CONFIG_EXECUTABLE)
    endif(PARSED_ARGS_CONFIG_EXECUTABLE)
    # </same if an external tool is provided and found>

    find_path(
        ${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_INCLUDE_DIR
        NAMES ${PARSED_ARGS_HEADER}
        PATH_SUFFIXES ${PARSED_ARGS_SUFFIXES}
        PATHS ${${PC_VAR_NS}_INCLUDE_DIRS} ${CONFIG_EXECUTABLE_INCLUDE_DIR} ${${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_ROOT_DIR}
    )

    if(${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_INCLUDE_DIR AND NOT ${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION AND EXISTS "${${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_INCLUDE_DIR}/${PARSED_ARGS_HEADER}")
        list(LENGTH PARSED_ARGS_VERSION_REGEXPES PARSED_ARGS_VERSION_REGEXPES_LENGTH)
        if(PARSED_ARGS_VERSION_REGEXPES_LENGTH EQUAL 0)
#             message(FATAL_ERROR "${__FUNCTION__}: argument VERSION_REGEXPES is not set")
        elseif(PARSED_ARGS_VERSION_REGEXPES_LENGTH EQUAL 1)
            # s'il n'y a qu'une regexp, c'est un numéro de version numérique (52301) ou sous forme dottée (5.23.01)
            file(STRINGS "${${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_INCLUDE_DIR}/${PARSED_ARGS_VERSION_HEADER}" VERSION_NUMBER_DEFINITION LIMIT_COUNT 1 REGEX ".*${PARSED_ARGS_VERSION_REGEXPES}.*")
            string(REGEX REPLACE ".*${PARSED_ARGS_VERSION_REGEXPES}.*" "\\1" VERSION_NUMBER "${VERSION_NUMBER_DEFINITION}")
            if(VERSION_NUMBER MATCHES "^[0-9]+$")
                set(i 1)
                set(MAJOR_DIVISOR 1)
                set(MINOR_DIVISOR 1)
                string(LENGTH "${VERSION_NUMBER}" VERSION_NUMBER_LENGTH)
                math(EXPR MINOR_DIGIT_COUNT "${VERSION_NUMBER_LENGTH} / 2")
                while(i LESS VERSION_NUMBER_LENGTH)
                    math(EXPR MAJOR_DIVISOR "${MAJOR_DIVISOR} * 10")
                    if(NOT i GREATER MINOR_DIGIT_COUNT)
                        math(EXPR MINOR_DIVISOR "${MINOR_DIVISOR} * 10")
                    endif(NOT i GREATER MINOR_DIGIT_COUNT)
                    math(EXPR i "${i} + 1")
                endwhile(i LESS VERSION_NUMBER_LENGTH)
                math(EXPR ${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION_MAJOR "${VERSION_NUMBER} / ${MAJOR_DIVISOR}")
                math(EXPR ${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION_MINOR "(${VERSION_NUMBER} - ${${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION_MAJOR} * ${MAJOR_DIVISOR}) / ${MINOR_DIVISOR}")
                math(EXPR ${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION_PATCH "${VERSION_NUMBER} - ${${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION_MAJOR} * ${MAJOR_DIVISOR} - ${${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION_MINOR} * ${MINOR_DIVISOR}")
                set(${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION "${${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION_MAJOR}.${${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION_MINOR}.${${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION_PATCH}")
            else(VERSION_NUMBER MATCHES "^[0-9]+$")
                set(${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION "${VERSION_NUMBER}")
                string(REGEX MATCHALL "[0-9]+" VERSION_PARTS "${VERSION_NUMBER}")
                if(VERSION_PARTS)
                    list(LENGTH VERSION_PARTS VERSION_PARTS_LENGTH)
                    list(GET VERSION_PARTS 0 ${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION_MAJOR)
                    if(VERSION_PARTS_LENGTH GREATER 1)
                        list(GET VERSION_PARTS 1 ${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION_MINOR)
                    endif(VERSION_PARTS_LENGTH GREATER 1)
                    if(VERSION_PARTS_LENGTH GREATER 2)
                        list(GET VERSION_PARTS 2 ${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION_PATCH)
                    endif(VERSION_PARTS_LENGTH GREATER 2)
                    if(VERSION_PARTS_LENGTH GREATER 3)
                        list(GET VERSION_PARTS 3 ${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION_TWEAK)
                    endif(VERSION_PARTS_LENGTH GREATER 3)
                endif(VERSION_PARTS)
            endif(VERSION_NUMBER MATCHES "^[0-9]+$")
        else() # > 1
            # s'il y en a plusieurs, les major/minor/patch sont sur plusieurs lignes/instructions (lire le fichier puis autant de string(REGEX REPLACE ...)
            file(READ "${${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_INCLUDE_DIR}/${PARSED_ARGS_VERSION_HEADER}" HEADER_CONTENT)
            list(GET PARSED_ARGS_VERSION_REGEXPES 0 VERSION_MAJOR_REGEXP)
            string(REGEX REPLACE ".*${VERSION_MAJOR_REGEXP}.*" "\\1" ${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION_MAJOR ${HEADER_CONTENT})
            set(${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION "${${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION_MAJOR}")
            if(PARSED_ARGS_VERSION_REGEXPES_LENGTH GREATER 1)
                list(GET PARSED_ARGS_VERSION_REGEXPES 1 VERSION_MINOR_REGEXP)
                string(REGEX REPLACE ".*${VERSION_MINOR_REGEXP}.*" "\\1" ${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION_MINOR ${HEADER_CONTENT})
                set(${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION "${${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION}.${${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION_MINOR}")
            endif(PARSED_ARGS_VERSION_REGEXPES_LENGTH GREATER 1)
            if(PARSED_ARGS_VERSION_REGEXPES_LENGTH GREATER 2)
                list(GET PARSED_ARGS_VERSION_REGEXPES 1 VERSION_PATCH_REGEXP)
                string(REGEX REPLACE ".*${VERSION_PATCH_REGEXP}.*" "\\1" ${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION_PATCH ${HEADER_CONTENT})
                set(${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION "${${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION}.${${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION_PATCH}")
            endif(PARSED_ARGS_VERSION_REGEXPES_LENGTH GREATER 2)
            if(PARSED_ARGS_VERSION_REGEXPES_LENGTH GREATER 3)
                list(GET PARSED_ARGS_VERSION_REGEXPES 1 VERSION_TWEAK_REGEXP)
                string(REGEX REPLACE ".*${VERSION_TWEAK_REGEXP}.*" "\\1" ${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION_TWEAK ${HEADER_CONTENT})
                set(${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION "${${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION}.${${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION_TWEAK}")
            endif(PARSED_ARGS_VERSION_REGEXPES_LENGTH GREATER 3)
        endif()
    endif(${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_INCLUDE_DIR AND NOT ${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION AND EXISTS "${${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_INCLUDE_DIR}/${PARSED_ARGS_HEADER}")

    if(MSVC)
        include(SelectLibraryConfigurations)
        set(POSSIBLE_DEBUG_NAMES )
        set(POSSIBLE_RELEASE_NAMES ${PARSED_ARGS_LIBRARIES})

        foreach(POSSIBLE_NAME ${PARSED_ARGS_LIBRARIES})
            list(APPEND POSSIBLE_DEBUG_NAMES "${POSSIBLE_NAME}d")
            if(DEFINED ${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION_MAJOR)
                list(APPEND POSSIBLE_DEBUG_NAMES "${POSSIBLE_NAME}${${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION_MAJOR}d")
                list(APPEND POSSIBLE_RELEASE_NAMES "${POSSIBLE_NAME}${${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION_MAJOR}")
                if(DEFINED ${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION_MINOR)
                    list(APPEND POSSIBLE_DEBUG_NAMES "${POSSIBLE_NAME}${${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION_MAJOR}${${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION_MINOR}d")
                    list(APPEND POSSIBLE_RELEASE_NAMES "${POSSIBLE_NAME}${${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION_MAJOR}${${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION_MINOR}")
                endif(DEFINED ${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION_MINOR)
            endif(DEFINED ${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION_MAJOR)
        endforeach(POSSIBLE_NAME)

        find_library(
            ${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_LIBRARY_RELEASE
            NAMES ${POSSIBLE_RELEASE_NAMES}
            PATH_SUFFIXES ${PARSED_ARGS_SUFFIXES}
            PATHS ${${PC_VAR_NS}_LIBRARY_DIRS} ${CONFIG_EXECUTABLE_LIBRARY} ${${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_ROOT_DIR}
        )
        find_library(
            ${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_LIBRARY_DEBUG
            NAMES ${POSSIBLE_DEBUG_NAMES}
            PATH_SUFFIXES ${PARSED_ARGS_SUFFIXES}
            PATHS ${${PC_VAR_NS}_LIBRARY_DIRS} ${CONFIG_EXECUTABLE_LIBRARY} ${${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_ROOT_DIR}
        )

        select_library_configurations("${PARSED_ARGS_OUTPUT_VARIABLE_NAME}")
    else(MSVC)
        find_library(
            ${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_LIBRARY
            NAMES ${PARSED_ARGS_LIBRARIES}
            PATH_SUFFIXES ${PARSED_ARGS_SUFFIXES}
            PATHS ${${PC_VAR_NS}_LIBRARY_DIRS} ${CONFIG_EXECUTABLE_LIBRARY} ${${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_ROOT_DIR}
        )
    endif(MSVC)

    # Check find_package arguments
    include(FindPackageHandleStandardArgs)
    if(${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_FIND_REQUIRED)
        find_package_handle_standard_args(
            ${PARSED_ARGS_OUTPUT_VARIABLE_NAME}
            REQUIRED_VARS ${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_LIBRARY ${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_INCLUDE_DIR
            VERSION_VAR ${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_VERSION
        )
    else(${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_FIND_REQUIRED)
        find_package_handle_standard_args(${PARSED_ARGS_OUTPUT_VARIABLE_NAME} "Could NOT find ${PARSED_ARGS_NAME}" ${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_LIBRARY ${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_INCLUDE_DIR)
    endif(${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_FIND_REQUIRED)

    if(${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_FOUND)
        set(VARIABLES_TO_EXPORT "FOUND" "VERSION" "VERSION_MAJOR" "VERSION_MINOR" "VERSION_PATCH")
        foreach(VARIABLE ${VARIABLES_TO_EXPORT})
            if(DEFINED ${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_${VARIABLE})
                set(${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_${VARIABLE} "${${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_${VARIABLE}}" PARENT_SCOPE)
            endif(DEFINED ${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_${VARIABLE})
        endforeach(VARIABLE)
        set(${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_LIBRARIES ${${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_LIBRARY} PARENT_SCOPE)
        set(${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_INCLUDE_DIRS ${${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_INCLUDE_DIR} PARENT_SCOPE)
        if(CMAKE_VERSION VERSION_GREATER "3.0.0")
            if(NOT TARGET ${PARSED_ARGS_NAME}::${PARSED_ARGS_NAME})
                add_library(${PARSED_ARGS_NAME}::${PARSED_ARGS_NAME} UNKNOWN IMPORTED)
            endif(NOT TARGET ${PARSED_ARGS_NAME}::${PARSED_ARGS_NAME})
            if(${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_LIBRARY_RELEASE)
                set_property(TARGET ${PARSED_ARGS_NAME}::${PARSED_ARGS_NAME} APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
                set_target_properties(${PARSED_ARGS_NAME}::${PARSED_ARGS_NAME} PROPERTIES IMPORTED_LOCATION_RELEASE "${${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_LIBRARY_RELEASE}")
            endif(${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_LIBRARY_RELEASE)
            if(${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_LIBRARY_DEBUG)
                set_property(TARGET ${PARSED_ARGS_NAME}::${PARSED_ARGS_NAME} APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
                set_target_properties(${PARSED_ARGS_NAME}::${PARSED_ARGS_NAME} PROPERTIES IMPORTED_LOCATION_DEBUG "${${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_LIBRARY_DEBUG}")
            endif(${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_LIBRARY_DEBUG)
            if(${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_LIBRARY)
                set_target_properties(${PARSED_ARGS_NAME}::${PARSED_ARGS_NAME} PROPERTIES IMPORTED_LOCATION "${${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_LIBRARY}")
            endif(${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_LIBRARY)
            set_target_properties(${PARSED_ARGS_NAME}::${PARSED_ARGS_NAME} PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_INCLUDE_DIR}")
        endif(CMAKE_VERSION VERSION_GREATER "3.0.0")
    endif(${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_FOUND)

    mark_as_advanced(
        ${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_INCLUDE_DIR
        ${PARSED_ARGS_OUTPUT_VARIABLE_NAME}_LIBRARY
    )

endfunction(common_find_package)
