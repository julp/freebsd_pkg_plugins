# history

A simple tool for troubleshooting: searching when/how/why a package was installed/removed/upgraded. This is **NOT** a tool designed for auditing/security.

## Examples

An extract of a pkg history:

```
pkg history

On 11/15/20 16:58:32: pkg install vim
        Operation            Package                                  New version          Old version          Repository
        Installed            cscope                                   15.9                 -                    FreeBSD
        Installed            ctags                                    5.8                  -                    FreeBSD
        Installed            vim                                      8.2.1943             -                    FreeBSD

On 11/15/20 16:12:22: pkg upgrade
        Operation            Package                                  New version          Old version          Repository
        Upgraded             firefox                                  83.0_2,2             83.0,2               FreeBSD
        Upgraded             nss                                      3.59                 3.58_1               FreeBSD
        Upgraded             poudriere                                3.3.6                3.3.5                FreeBSD
```

Search when firefox has been upgraded:

```
pkg history -u firefox

On 11/15/20 16:12:22: pkg upgrade
        Operation            Package                                  New version          Old version          Repository
        Upgraded             firefox                                  83.0_2,2             83.0,2               FreeBSD

On 11/13/20 15:20:58: pkg upgrade firefox poudriere
        Operation            Package                                  New version          Old version          Repository
        Upgraded             firefox                                  83.0,2               82.0.3,2             FreeBSD
```
