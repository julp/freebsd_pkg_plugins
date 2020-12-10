# services

The real prupose of this module was to restart services which depends on openssl after a security upgrade of the last one.

But, note that only services that were built with a version of openssl (including forks like libressl) from the ports are concerned. The services relying on openssl from base/world won't be restarted!
