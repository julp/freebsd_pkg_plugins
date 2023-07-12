# integrity

A plugin to check if a file installed from a package has been since altered prior its deletion/replacement (before pkg upgrade/deinstall/autoremove).

Could be usefull in a "paranoïd" environment to trigger some red flag.

Important note: this plugin won't be helpful if pkg's database itself is altered!
