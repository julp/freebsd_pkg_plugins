# zint (zfs_integration)

Provides ZFS and BE support.

Status: **experimental** (use it at your own risk), beta testers welcome.

Automatically takes a snapshot (ZFS) or creates a boot environment (BE) before actually running `pkg upgrade` or `pkg autoremove` (by default, events when to do so are configurable).

Also provides a `pkg zint rollback` command to revert your active boot environment to the latest BE (ZFS not supported, only BE - for now) created by this plugin (of course, you need to reboot to apply the change) allowing you to return prior to `pkg upgrade`/`pkg autoremove` if something turned really bad.

The plugin automatically choose between BE and ZFS by probing if /, /usr/local and /var/db/pkg are all on ZFS and are on the same file systems. If this is the case, BE is used else ZFS if at least /usr/local is a ZFS file system. The snapshot will be recursive if / is also on ZFS and on the same pool as /usr/local.

Example from my own desktop machine:

```
# bectl list
BE                                  Active Mountpoint Space Created
default                             NR     /          5.29G 2021-07-25 22:13
pkg_pre_upgrade_2021-09-06_16:18:57 -      -          217M  2021-09-06 16:18
pkg_pre_upgrade_2021-09-09_16:10:15 -      -          144M  2021-09-09 16:10
pkg_pre_upgrade_2021-09-11_15:06:25 -      -          40.7M 2021-09-11 15:06
pkg_pre_upgrade_2021-09-14_16:21:24 -      -          41.3M 2021-09-14 16:21
```

## Configuration

Note: keys are case sensitive, they have to be uppercased in ```\`pkg config PLUGINS_CONF_DIR\`/zint.conf```

* `FORCE` (boolean, default: `false`): when `false`, do not create a BE/snapshot if the `pkg` command does not actually imply any change. Set it to `true` to create a BE/snapshot anyway, you might want to turn it on if you run more often `pkg` than you create BE/snapshot
* `RETENTION` (default: disabled):
  * with an integer, for example `RETENTION = "7";`, only keep the 7 latest BE/snapshots
  * with a string, for example `RETENTION = "2 weeks";` delete the BE/snapshots older than 2 weeks
* `ON` (default: `[ pre_upgrade, pre_autoremove ]`), an array of events from the list below :
  + `pre_install`: to create a BE/snapshot right before `pkg install`
  + `post_install`: right after `pkg install`
  + `pre_deinstall`: before `pkg remove`
  + `post_deinstall`: after `pkg remove`
  + `pre_upgrade`: before `pkg upgrade`
  + `post_upgrade`: after `pkg upgrade`
  + `pre_autoremove`: before `pkg autoremove`
  + `post_autoremove`: after `pkg autoremove`

By default, BE/snapshot are named from the `strftime` pattern `pkg_<event name>_%F_%T` but you can override this pattern by using an object where the keys are the event's name from the list above and the value, your own strftime-pattern. Example:

```
ON: {
    pre_upgrade: "zint_upgrade_%Y-%m-%d-%H:%i:%s",
    pre_autoremove: "zint_autoremove_%Y-%m-%d-%H:%i:%s",
}
```

## Important note

ports-mgmt/pkg sticks to the type of the default value registered by `pkg_plugin_conf_add` so `ON` has to be an array (it can't be an object) and `RETENTION` has to be a string (not an integer). To leverage this limitation, recompile `pkg` after copying the patch included in this directory to /usr/ports/ports-mgmt/pkg/files

```
cp patch-libpkg_plugins.c /usr/ports/ports-mgmt/pkg/files
make -C /usr/ports/ports-mgmt/pkg deinstall
make -C /usr/ports/ports-mgmt/pkg distclean
make -C /usr/ports/ports-mgmt/pkg install
```
