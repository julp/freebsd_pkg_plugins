cmake_minimum_required(VERSION 2.8.3...3.20.5)

add_subdirectory(history)
if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    add_subdirectory(services)
    add_subdirectory(zfs_integration)
endif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
add_subdirectory(verbose)
