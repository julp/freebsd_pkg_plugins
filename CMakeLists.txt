cmake_minimum_required(VERSION 2.8.3)

project(pkg_plugins C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wwrite-strings -Wstrict-prototypes -Wuninitialized -Wunreachable-code -Wno-comment -Wnonnull -Wunreachable-code")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG")

#message("CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")
#message("CMAKE_C_FLAGS_DEBUG = ${CMAKE_C_FLAGS_DEBUG}")

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})

find_package(pkg REQUIRED)

function(println)
    message("==============================")
endfunction(println)

function(debug PREFIX)
    set(SUFFIXES "FOUND" "VERSION" "VERSION_MAJOR" "VERSION_MINOR" "VERSION_PATCH" "LIBRARY" "LIBRARIES" "INCLUDE_DIR" "INCLUDE_DIRS")
#     list(APPEND SUFFIXES "FIND_VERSION" "FIND_VERSION_EXACT" "FIND_COMPONENTS" "FIND_REQUIRED" "FIND_QUIETLY")
    if(MSVC)
        list(APPEND SUFFIXES "LIBRARY_RELEASE" "LIBRARY_DEBUG")
    endif(MSVC)
    foreach(SUFFIX ${SUFFIXES})
        if(DEFINED ${PREFIX}_${SUFFIX})
            message("${PREFIX}_${SUFFIX} = ${${PREFIX}_${SUFFIX}}")
        else(DEFINED ${PREFIX}_${SUFFIX})
            message("${PREFIX}_${SUFFIX} = <UNDEFINED>")
        endif(DEFINED ${PREFIX}_${SUFFIX})
    endforeach(SUFFIX)
    println()
endfunction(debug)

add_subdirectory(error)
add_subdirectory(libparallelize)
add_subdirectory(sqlite)
add_subdirectory(plugins)
