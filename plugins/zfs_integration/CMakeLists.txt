cmake_minimum_required(VERSION 2.8.3...3.20.5)

set(HAVE_BE false)

find_path(
    BE_INCLUDE_DIR
    NAMES be.h
)

if(BE_INCLUDE_DIR)
    find_library(
        BE_LIBRARY
        NAMES be
    )
endif(BE_INCLUDE_DIR)

#message("BE_LIBRARY = ${BE_LIBRARY}")
#message("BE_INCLUDE_DIR = ${BE_INCLUDE_DIR}")

set(ZFS_SOURCES
    zfs.c
    probe.c
    raw_zfs_method.c
)

set(ZFS_INCLURE_DIRS
    # FreeBSD 13
    /usr/src/sys/contrib/openzfs/include/ # libzfs.h + libuutil.h + libzfs_core.h + libnvpair.h + zfs_prop.h
    /usr/src/sys/contrib/openzfs/lib/libspl/include/ # umem.h + sys/types.h
    /usr/src/sys/contrib/openzfs/lib/libspl/include/os/freebsd/sys/ # mnttab.h
    /usr/src/sys/cddl/compat/opensolaris/ # mnttab.h
    # FreeBSD 12
    /usr/src/cddl/contrib/opensolaris/lib/libzpool/common/ # sys/zfs_context.h ?
    /usr/src/cddl/compat/opensolaris/include/ # mnttab.h
    /usr/src/cddl/compat/opensolaris/lib/libumem/ # umem.h
    /usr/src/sys/cddl/compat/opensolaris/ # for its sys/types.h ?
    /usr/src/cddl/contrib/opensolaris/head/ # ? https://svnweb.freebsd.org/base/releng/12.2/cddl/contrib/opensolaris/head/
    /usr/src/cddl/contrib/opensolaris/lib/libuutil/common/ # libuutil.h
    /usr/src/cddl/contrib/opensolaris/lib/libzfs/common/ # libzfs.h
    /usr/src/cddl/contrib/opensolaris/lib/libzfs_core/common/ # libzfs_core.h
    /usr/src/cddl/contrib/opensolaris/lib/libumem/common/ # never existed ?
    /usr/src/cddl/contrib/opensolaris/lib/libnvpair/ # libnvpair.h
    /usr/src/sys/cddl/contrib/opensolaris/uts/common/ # no .h ?
    /usr/src/sys/cddl/contrib/opensolaris/uts/common/fs/zfs/ # zfeature.h ?
    /usr/src/sys/cddl/contrib/opensolaris/uts/common/sys/ # nvpair.h ?
    /usr/src/sys/cddl/contrib/opensolaris/common/zfs/ # zfs_prop.h
)

#string(REGEX MATCH "^[0-9]+" MAJOR_SYSTEM_VERSION "${CMAKE_SYSTEM_VERSION}")
#if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD" AND MAJOR_SYSTEM_VERSION GREATER 11)
if(BE_INCLUDE_DIR AND BE_LIBRARY)
    set(HAVE_BE true)
    list(APPEND ZFS_SOURCES be_method.c)
    #list(APPEND ZINT_LIBRARIES ${BE_LIBRARY})
endif(BE_INCLUDE_DIR AND BE_LIBRARY)
#endif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD" AND MAJOR_SYSTEM_VERSION GREATER 11)

#get_target_property(INHERITED_INCLUDE_DIRECTORIES sqlite INCLUDE_DIRECTORIES)
#list(APPEND ZFS_INCLURE_DIRS ${INHERITED_INCLUDE_DIRECTORIES})
list(APPEND ZFS_INCLURE_DIRS "${PROJECT_SOURCE_DIR}")
list(APPEND ZFS_INCLURE_DIRS "${PROJECT_SOURCE_DIR}/kissc")
list(APPEND ZFS_INCLURE_DIRS "${PROJECT_SOURCE_DIR}/error")
#list(APPEND ZFS_INCLURE_DIRS "${PROJECT_SOURCE_DIR}/shared")

# isolate zfs part which causes some noises/conflicts
add_library(zfs OBJECT EXCLUDE_FROM_ALL ${ZFS_SOURCES})
# solve conflict on boolean_t between /usr/include/vm/vm.h and /usr/src/sys/cddl/compat/opensolaris/sys/types.h
get_target_property(ZFS_SOURCES zfs SOURCES)
foreach(source ${ZFS_SOURCES})
    set_source_files_properties(${source} PROPERTIES
        COMPILE_DEFINITIONS "NEED_SOLARIS_BOOLEAN"
    )
endforeach(source)
set_target_properties(zfs PROPERTIES
    INCLUDE_DIRECTORIES "${ZFS_INCLURE_DIRS}"
    COMPILE_FLAGS "-fPIC"
)

set(ZINT_SOURCES
    plugin_zfs_integration.c
    #utils.c
    none_method.c
    $<TARGET_OBJECTS:zfs>
    ${PROJECT_SOURCE_DIR}/shared/os.c
    ${PROJECT_SOURCE_DIR}/shared/path_join.c
    ${PROJECT_SOURCE_DIR}/kissc/stpcpy_sp.c
)
set(ZINT_LIBRARIES
    kvm
    zfs
    $<TARGET_OBJECTS:error>
)
set(ZINT_INCLUDE_DIRECTORIES
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/error
    ${PROJECT_SOURCE_DIR}/kissc
    ${PROJECT_SOURCE_DIR}/shared
)
if(BE_INCLUDE_DIR AND BE_LIBRARY)
    list(APPEND ZINT_LIBRARIES ${BE_LIBRARY})
endif(BE_INCLUDE_DIR AND BE_LIBRARY)

pkg_plugin(
    INSTALL
    NAME zint
    SOURCES ${ZINT_SOURCES}
    LIBRARIES ${ZINT_LIBRARIES}
    INCLUDE_DIRECTORIES ${ZINT_INCLUDE_DIRECTORIES}
)

configure_file(
    "config.h.in"
    "${CMAKE_BINARY_DIR}/config.h"
    @ONLY
)
