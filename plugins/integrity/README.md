# integrity

A plugin to check if a file installed from a package has been since altered prior its deletion/replacement (before pkg upgrade/deinstall/autoremove).

Could be usefull in a "paranoïd" environment to trigger some red flag.

Important notes:

* this plugin won't be helpful if pkg's database itself was altered!
* this plugin will greatly slow down the delete and upgrade operations
