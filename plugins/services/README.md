# services

The real purpose of this module was to restart services which depends on openssl after a security upgrade of the last one.

But, note that only services that were built with a version of openssl (including forks like libressl) from the ports are concerned. The services relying on openssl from base/world won't be restarted!

```
pkg upgrade
# ...
services: services not restarted due to blocklist: sddm
services: services successfully restarted: nginx
# ...
```

Default blocklist (services to never restart automatically) is: `sddm, hald, dbus`

To configure your own:

```
cat > /usr/local/etc/pkg/services.conf <<EOF
BLOCKLIST: [
  sddm,
  hald,
  dbus,
]
EOF
```

Note: keys are case sensitive, they have to be uppercased in ```\`pkg config PLUGINS_CONF_DIR\`/services.conf```

Services from deinstalled packages are also automatically stopped before removal.

Also provides commands `pkg services` and `pkg rcorder`.

```
pkg services -r expat
The package expat is required by the following service(s):
- avahi-daemon (/usr/local/etc/rc.d/avahi-daemon)
- dbus (/usr/local/etc/rc.d/dbus)
- avahi-dnsconfd (/usr/local/etc/rc.d/avahi-dnsconfd)
- hald (/usr/local/etc/rc.d/hald)
- git_daemon (/usr/local/etc/rc.d/git_daemon)
- svnserve (/usr/local/etc/rc.d/svnserve)
- unbound (/usr/local/etc/rc.d/unbound)

pkg services unbound
The package unbound provides the following service(s):
- unbound (/usr/local/etc/rc.d/unbound)
```
