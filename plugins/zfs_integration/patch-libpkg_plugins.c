--- libpkg/plugins.c.orig	2022-09-28 16:40:18 UTC
+++ libpkg/plugins.c
@@ -367,10 +367,12 @@ pkg_plugin_parse(struct pkg_plugin *p)
 		if (o == NULL)
 			continue;
 
+#if 0
 		if (o->type != cur->type) {
 			pkg_emit_error("Malformed key %s, ignoring", key);
 			continue;
 		}
+#endif
 
 		ucl_object_delete_key(p->conf, key);
 		ucl_object_insert_key(p->conf, ucl_object_ref(cur), key, strlen(key), false);
