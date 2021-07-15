cmake_minimum_required(VERSION 2.8.3...3.20.5)

add_library(error OBJECT #[[EXCLUDE_FROM_ALL]] error.c)
set_target_properties(error PROPERTIES
    COMPILE_FLAGS "-fPIC"
)
