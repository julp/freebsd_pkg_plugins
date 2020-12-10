# FreeBSD pkg plugins

## Disclaimer

* I decline all responsabilities of the usage made of these plugins, there is absolutely no warranty. They are provided as is
* only FreeBSD (stable) is supported (for now?)

## Prerequisites

* cmake
* a C99 compiler
* re2c >= 2.0.0 (note: not available in FreeBSD's ports)
* pkg >= 1.5.10
* sqlite3 (history)
* FreeBSD sources (zfs_integration)

## Installation

Not (yet) in ports tree, so:

```
# as a regular user
git clone https://github.com/julp/freebsd_pkg_plugins.git
cmake -S freebsd_pkg_plugins -B /tmp/freebsd_pkg_plugins -DCMAKE_BUILD_TYPE=Debug
make -C /tmp/freebsd_pkg_plugins

# then, as root
make -C /tmp/freebsd_pkg_plugins install
mkdir -p `pkg config pkg_plugins_dir`
cat >> /usr/local/etc/pkg.conf <<EOF
PLUGINS: [
    zfsint,
    history,
    services,
]
EOF
```

## Plugins

* zint (zfs_integration): snapshots (via be or raw ZFS - other filesystems not supported) before `pkg upgrade`
* services: manages services from pkg, stop them before deletion and restart them after upgrade (of themselves or a shlib dependency)
* history: keep tracks of all the pkg install/delete/upgrade/autoremove commands you run with the operations on concerned packages and a queryable database

See README.md in subdirectory of plugins/ for further details of each one of these.
