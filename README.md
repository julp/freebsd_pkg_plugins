# FreeBSD pkg plugins

## Disclaimer

* I decline all responsabilities of the usage made of these plugins, there is absolutely no warranty. They are provided as is
* only FreeBSD (stable) is supported (for now?)

## Prerequisites

* cmake
* a C99 compiler
* pkg >= 1.5.10
* sqlite3 (history)
* FreeBSD sources (zfs_integration)

optional:

* re2c >= 2.0.0 (history)

## Installation

Not (yet) in ports tree, so:

```
# as a regular user
git clone https://github.com/julp/freebsd_pkg_plugins.git
cmake -S freebsd_pkg_plugins -B /tmp/freebsd_pkg_plugins # -DCMAKE_BUILD_TYPE=Debug

# to build on FreeBSD as a port/package and pretending I previously was the user julp and ran git clone in my home
# as root
echo 'OVERLAYS+=/home/julp/freebsd_pkg_plugins/usr/ports' >> /etc/make.conf
# replace pkg_zint by the desired plugin (pkg_history or pkg_services)
# (have to be run as root if /usr/ports/distfiles is not writable to user)
make -C /home/julp/freebsd_pkg_plugins/usr/ports/ports-mgmt/pkg_zint makesum
# back to regular user
make -C /home/julp/freebsd_pkg_plugins/usr/ports/ports-mgmt/pkg_zint
# then, as root
make -C /home/julp/freebsd_pkg_plugins/usr/ports/ports-mgmt/pkg_zint install

# generic procedure
make -C /tmp/freebsd_pkg_plugins
# then, as root
make -C /tmp/freebsd_pkg_plugins install
mkdir -p `pkg config pkg_plugins_dir`
cat >> /usr/local/etc/pkg.conf <<EOF
PLUGINS: [
    zint,
    history,
    services,
    integrity,
]
EOF
```

## Plugins

* zint (zfs_integration): snapshots (via be or "raw ZFS" - other filesystems not supported) before pkg operations (default is upgrade and autoremove)
* services: manages services from pkg, stop them before deletion and restart them after upgrade (of themselves or a shlib dependency)
* history: keep tracks of all the pkg install/delete/upgrade/autoremove commands you run with the operations on concerned packages and a queryable database
* integrity (experimental): checks integrity of deleted (and upgraded) files

See README.md in subdirectory of plugins/ for further details of each one of these.

## Known issue

You may have to disable your plugins first in order to upgrade them when pkg is also updated.
