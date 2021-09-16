# zint (zfs_integration)

Provides ZFS and BE support.

Status: **experimental** (use it at your own risk), beta testers welcome.

Automatically takes a snapshot (ZFS) or creates a boot environment (BE) before actually running `pkg upgrade` of the name pkg\_pre\_upgrade\_\<date>\_\<time>.

Also provides a `pkg zint rollback` command to revert your active boot environment to the latest pkg\_pre\_upgrade\_\* BE (ZFS not supported, only BE - for now) (of course, you need to reboot to apply the change) allowing you to return prior to `pkg upgrade` if something turned really bad.

The plugin automatically choose between BE and ZFS by probing if /, /usr/local and /var/db/pkg are all on ZFS and are on the same file systems. If this is the case, BE is used else ZFS if at least /usr/local is a ZFS file system. The snapshot will be recursive if / is also on ZFS and on the same pool as /usr/local.

```
# bectl list
BE                                  Active Mountpoint Space Created
default                             NR     /          5.29G 2021-07-25 22:13
pkg_pre_upgrade_2021-09-06_16:18:57 -      -          217M  2021-09-06 16:18
pkg_pre_upgrade_2021-09-09_16:10:15 -      -          144M  2021-09-09 16:10
pkg_pre_upgrade_2021-09-11_15:06:25 -      -          40.7M 2021-09-11 15:06
pkg_pre_upgrade_2021-09-14_16:21:24 -      -          41.3M 2021-09-14 16:21
```
