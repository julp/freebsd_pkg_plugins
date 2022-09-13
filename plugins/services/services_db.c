#include <sys/param.h> /* __FreeBSD__ */
#include <stdlib.h>
#include <pkg.h>
#include <search.h>
#include <dirent.h>
#include <sys/stat.h> /* stat(2) */
#include <libutil.h> /* fparseln(3) */
#include <spawn.h> /* posix_spawn(3) */
#include <stdint.h>
#include <inttypes.h>

#include "common.h"
#include "error.h"
#include "shared/os.h"
#include "shared/path_join.h"
#include "kissc/stpcpy_sp.h"
#include "kissc/dlist.h"
#include "kissc/hashtable.h"
#include "services.h"
#include "private_rcorder.h"

struct services_db_t {
    DList roots; // DList<rc_d_script_t *>
    HashTable scripts; // const char * => DList<rc_d_script_t *>
    HashTable provides; // const char * => DList<rc_d_script_t *>
    HashTable keywords; // const char * => DList<rc_d_script_t *>
    HashTable packages; // const char * => package_t *
};

typedef struct {
    const char *name;
    DList rshlibs; // DList<rc_d_script_t *>
    DList scripts; // DList<rc_d_script_t *>
} package_t;

struct rc_d_script_t {
    const char *name;
    const char *path;
    package_t *package;
    DList befores; // DList<const char *>
    DList requires; // DList<const char *>
    DList keywords; // DList<const char *>
    DList children; // DList<rc_d_script_t *> same as requires but after "resolution" of names
    DList parents; // DList<rc_d_script_t *> same as befores but after "resolution" of names
};

typedef struct {
    const char *name;
    DList scripts; // DList<rc_d_script_t *>
} keyword_t;

static const void *empty_iterator_data[] = { NULL };

static bool scan_rc_d_directory(struct pkgdb *, services_db_t *, const char *, char **);

static void keyword_destroy(keyword_t *kw)
{
    dlist_clear(&kw->scripts);
    free((void *) kw->name);
    free(kw);
}

static void script_destroy(rc_d_script_t *script)
{
    dlist_clear(&script->parents);
    dlist_clear(&script->befores);
    dlist_clear(&script->requires);
    dlist_clear(&script->keywords);
    dlist_clear(&script->children);
    free((void *) script->path);
    // do NOT free script->name
    free(script);
}

static package_t *package_create(const char *name, char **error)
{
    package_t *pkg;

    assert(NULL != name);

    if (NULL == (pkg = malloc(sizeof(*pkg)))) {
        set_malloc_error(error, sizeof(*pkg));
    } else {
        dlist_init(&pkg->scripts, NULL, NULL);
        dlist_init(&pkg->rshlibs, NULL, NULL);
        pkg->name = strdup(name);
    }

    return pkg;
}

static void package_destroy(package_t *pkg)
{
    dlist_clear(&pkg->rshlibs);
    dlist_clear(&pkg->scripts);
    free((void *) pkg->name);
    free(pkg);
}

static void provide_destroy(DList *scripts)
{
    dlist_clear(scripts);
    free(scripts);
}

static void dlist_insert_unique(DList *list, CmpFunc cmpfn, void *data)
{
    if (NULL == dlist_find_first(list, cmpfn, data)) {
        dlist_append(list, data, NULL);
    }
}

services_db_t *services_db_create(char **error)
{
    services_db_t *db;

    if (NULL == (db = malloc(sizeof(*db)))) {
        set_malloc_error(error, sizeof(*db));
    } else {
        dlist_init(&db->roots, NULL, NULL);
        hashtable_ascii_cs_init(&db->scripts, NULL, NULL, (DtorFunc) script_destroy);
        hashtable_ascii_cs_init(&db->keywords, NULL, NULL, (DtorFunc) keyword_destroy);
        hashtable_ascii_cs_init(&db->provides, (DupFunc) strdup, (DtorFunc) free, (DtorFunc) provide_destroy);
        hashtable_ascii_cs_init(&db->packages, NULL, NULL, (DtorFunc) package_destroy);
    }

    return db;
}

void services_db_close(services_db_t *db)
{
    assert(NULL != db);

    dlist_clear(&db->roots);
    hashtable_destroy(&db->scripts);
    hashtable_destroy(&db->keywords);
    hashtable_destroy(&db->provides);
    hashtable_destroy(&db->packages);
}

pkg_error_t services_db_load_from_cache(const char *UNUSED(path), services_db_t **UNUSED(db), char **UNUSED(error))
{
    // TODO: to be implemented in the future?

    return EPKG_ENODB;
}

bool services_db_dump_to_cache(const char *UNUSED(path), services_db_t *UNUSED(db), char **UNUSED(error))
{
    // TODO: to be implemented in the future?

    return true;
}

static int compare_scripts(rc_d_script_t *a, rc_d_script_t *b)
{
    return strcmp(a->name, b->name);
}

static bool provides_by_name(services_db_t *db, const char *name, DList **scripts)
{
    return hashtable_get(&db->provides, name, scripts);
}

static void set_parenthood(rc_d_script_t *parent, rc_d_script_t *child)
{
    dlist_insert_unique(&parent->children, (CmpFunc) compare_scripts, child);
    dlist_insert_unique(&child->parents, (CmpFunc) compare_scripts, parent);
}

static void add_before_relationship(services_db_t *db, rc_d_script_t *parent, const char *child_name)
{
    DList *children;

    if (provides_by_name(db, child_name, &children)) {
        Iterator it;
        rc_d_script_t *child;

        dlist_to_iterator(&it, children);
        for (iterator_first(&it); iterator_is_valid(&it, NULL, &child); iterator_next(&it)) {
            set_parenthood(parent, child);
        }
        iterator_close(&it);
    } else {
        debug("(child) %s not found", child_name);
    }
}

static void add_require_relationship(services_db_t *db, rc_d_script_t *child, const char *parent_name)
{
    DList *parents;

    if (provides_by_name(db, parent_name, &parents)) {
        Iterator it;
        rc_d_script_t *parent;

        dlist_to_iterator(&it, parents);
        for (iterator_first(&it); iterator_is_valid(&it, NULL, &parent); iterator_next(&it)) {
            set_parenthood(parent, child);
        }
        iterator_close(&it);
    } else {
        debug("(parent) %s not found", parent_name);
    }
}

bool services_db_scan_system(struct pkgdb *pkgdb, services_db_t *db, char **error)
{
    bool ok;
    Iterator it;
    rc_d_script_t *script;
#if defined(__FreeBSD__)
    char rc_d_directory[MAXPATHLEN];
#endif /* FreeBSD */

    assert(NULL != pkgdb);
    assert(NULL != db);

    ok = false;
    do {
        if (!scan_rc_d_directory(pkgdb, db, "/etc/rc.d", error)) {
            break;
        }
#if defined(__FreeBSD__)
        if (!path_join(rc_d_directory, rc_d_directory + STR_SIZE(rc_d_directory), error, localbase(), "etc/rc.d", NULL)) {
            break;
        }
        if (!scan_rc_d_directory(pkgdb, db, rc_d_directory, error)) {
            break;
        }
#endif /* FreeBSD */
        // first step, now all scripts were parsed, manage relationships (REQUIRE/BEFORE) between them
        hashtable_to_iterator(&it, &db->scripts);
        for (iterator_first(&it); iterator_is_valid(&it, NULL, &script); iterator_next(&it)) {
            Iterator itp;
            const char *name;

            dlist_to_iterator(&itp, &script->requires);
            for (iterator_first(&itp); iterator_is_valid(&itp, NULL, &name); iterator_next(&itp)) {
                add_require_relationship(db, script, name);
            }
            iterator_close(&itp);
            dlist_to_iterator(&itp, &script->befores);
            for (iterator_first(&itp); iterator_is_valid(&itp, NULL, &name); iterator_next(&itp)) {
                add_before_relationship(db, script, name);
            }
            iterator_close(&itp);
        }
        iterator_close(&it);
        // second step, identify "roots" (the scripts without "parents" - not REQUIREd by any other script)
        hashtable_to_iterator(&it, &db->scripts);
        for (iterator_first(&it); iterator_is_valid(&it, NULL, &script); iterator_next(&it)) {
            if (dlist_empty(&script->parents)) {
//                 debug("[ROOT] %s", script->name);
                dlist_append(&db->roots, script, NULL);
            }
        }
        iterator_close(&it);
        ok = true;
    } while (false);

    return ok;
}

static bool keep_script(rc_d_script_t *script, rcorder_options_t *ro)
{
    Iterator it;
    keyword_t *kw;
    intptr_t action;

    action = RCORDER_ACTION_NONE;
    dlist_to_iterator(&it, &script->keywords);
    for (iterator_first(&it); iterator_is_valid(&it, NULL, &kw) && RCORDER_ACTION_SKIP != action; iterator_next(&it)) {
        hashtable_get(&ro->ks, kw->name, &action);
    }
    iterator_close(&it);

    return RCORDER_ACTION_KEEP == action || (RCORDER_ACTION_NONE == action && 0 == ro->keep_count);
}

static void visit_script_bis(HashTable *visited, rc_d_script_t *script, rcorder_options_t *ro, void (*callback)(const rc_d_script_t *script, void *user_data), void *user_data)
{
    bool keep;

    hashtable_put(visited, 0, script, NULL, NULL);
    keep = (ro->include_orphans || NULL != script->package) && keep_script(script, ro);
    if (keep && ro->reverse) {
        callback(script, user_data);
    }
    if (!dlist_empty(&script->children)) {
        Iterator it;
        rc_d_script_t *child;

        dlist_to_iterator(&it, &script->children);
        for (iterator_first(&it); iterator_is_valid(&it, NULL, &child); iterator_next(&it)) {
            if (!hashtable_direct_contains(visited, child)) {
                visit_script_bis(visited, child, ro, callback, user_data);
            }
        }
        iterator_close(&it);
    }
    if (keep && !ro->reverse) {
        callback(script, user_data);
    }
}

void services_db_rcorder_iter(services_db_t *db, rcorder_options_t *ro, void (*callback)(const rc_d_script_t *script, void *user_data), void *user_data)
{
    Iterator it;
    HashTable visited;
    rc_d_script_t *script;

    assert(NULL != db);
    assert(NULL != ro);
    assert(NULL != callback);

    hashtable_init(&visited, hashtable_size(&db->scripts), NULL, NULL, NULL, NULL, NULL);
    dlist_to_iterator(&it, &db->roots);
    for (iterator_first(&it); iterator_is_valid(&it, NULL, &script); iterator_next(&it)) {
        visit_script_bis(&visited, script, ro, callback, user_data);
    }
    iterator_close(&it);
    hashtable_destroy(&visited);
}

static void handle_before(services_db_t *UNUSED(db), rc_d_script_t *script, const char *token)
{
    dlist_append(&script->befores, strdup(token), NULL);
}

static void handle_require(services_db_t *UNUSED(db), rc_d_script_t *script, const char *token)
{
    dlist_append(&script->requires, strdup(token), NULL);
}

static void handle_provide(services_db_t *db, rc_d_script_t *script, const char *token)
{
    bool put;
    ht_hash_t h;
    DList *scripts;

    h = hashtable_hash(&db->provides, token);
    if (!hashtable_quick_get(&db->provides, h, token, &scripts)) {
        scripts = malloc(sizeof(*scripts));
        assert(NULL != scripts);
        dlist_init(scripts, NULL, NULL);
        put = hashtable_quick_put(&db->provides, HT_PUT_ON_DUP_KEY_PRESERVE, h, strdup(token), scripts, NULL);
        assert(put);
    }
    put = dlist_append(scripts, script, NULL);
    assert(put);
}

static void handle_keyword(services_db_t *db, rc_d_script_t *script, const char *token)
{
    bool put;
    ht_hash_t h;
    keyword_t *kw;

    h = hashtable_hash(&db->keywords, token);
    if (!hashtable_quick_get(&db->keywords, h, token, &kw)) {
        kw = malloc(sizeof(*kw));
        assert(NULL != kw);
        dlist_init(&kw->scripts, NULL, NULL);
        kw->name = strdup(token);
        put = hashtable_quick_put(&db->keywords, HT_PUT_ON_DUP_KEY_PRESERVE, h, kw->name, kw, NULL);
        assert(put);
    }
    put = dlist_append(&kw->scripts, script, NULL);
    assert(put);
    dlist_append(&script->keywords, kw, NULL);
}

static const struct {
    const char *bol;
    size_t bol_len;
    void (*handle_token)(services_db_t *db, rc_d_script_t *script, const char *token);
} rc_d_magic_comments[] = {
  { S("# BEFORE:"), handle_before },
  { S("# REQUIRE:"), handle_require },
  { S("# REQUIRES:"), handle_require },
  { S("# PROVIDE:"), handle_provide },
  { S("# PROVIDES:"), handle_provide },
  { S("# KEYWORD:"), handle_keyword },
  { S("# KEYWORDS:"), handle_keyword },
};

// NOTE: a rc.d script can have 0 as several PROVIDE(S)
static bool parse_rc_d_script(services_db_t *db, rc_d_script_t *script, char **error)
{
    bool ok;

    assert(NULL != db);
    assert(NULL != script);

    ok = false;
    do {
        FILE *fp;
        int state;
        char *line;
        char delims[] = { '\\', '\\', '\0' };
        enum { PARSING_BEFORE, PARSING_PENDING, PARSING_DONE };

        if (NULL == (fp = fopen(script->path, "r"))) {
            set_generic_error(error, "can't fopen(3) %s", script->path);
            break;
        }
        state = PARSING_BEFORE;
        while (NULL != (line = fparseln(fp, NULL, NULL, delims, 0)) && PARSING_DONE != state) {
            size_t i;

            if (PARSING_BEFORE != state) {
                state = PARSING_DONE;
            }
            for (i = 0; i < ARRAY_SIZE(rc_d_magic_comments); i++) {
                if (0 == strncmp(rc_d_magic_comments[i].bol, line, rc_d_magic_comments[i].bol_len)) {
                    char *parsing_start_point, *token;

                    state = PARSING_PENDING;
                    parsing_start_point = line + rc_d_magic_comments[i].bol_len;
                    while (NULL != (token = strsep(&parsing_start_point, " \t\n"))) {
                        if ('\0' != *token) {
                            rc_d_magic_comments[i].handle_token(db, script, token);
                        }
                    }
                    break;
                }
            }
            free(line);
        }
        fclose(fp);
        ok = true;
    } while (false);

    return ok;
}

static package_t *package_retrieve(services_db_t *db, const char *name, char **error)
{
    package_t *pkg;

    assert(NULL != db);
    assert(NULL != name);

    do {
        ht_hash_t h;

        h = hashtable_hash(&db->packages, name);
        if (!hashtable_quick_get(&db->packages, h, name, &pkg)) {
            if (NULL == (pkg = package_create(name, error))) {
                break;
            }
            if (!hashtable_quick_put(&db->packages, HT_PUT_ON_DUP_KEY_PRESERVE, h, pkg->name, pkg, NULL)) {
                set_generic_error(error, "referencing package '%s' failed", name);
                package_destroy(pkg);
                pkg = NULL;
                break;
            }
        }
    } while (false);

    return pkg;
}

static bool associate_package_to_script(services_db_t *db, rc_d_script_t *script, package_t *pkg, char **error)
{
    bool ok;

    assert(NULL != db);
    assert(NULL != script);
    assert(NULL != pkg);

    ok = false;
    do {
        if (!dlist_append(&pkg->scripts, script, NULL)) {
            set_generic_error(error, "linking package '%s' to script '%s' failed", pkg->name, script->name);
            break;
        }
        script->package = pkg;
        ok = true;
    } while (false);

    return ok;
}

static bool pkg_from_rc_d_script(struct pkgdb *pkg_db, services_db_t *db, rc_d_script_t *script, char **error)
{
    bool ok;

    assert(NULL != pkg_db);
    assert(NULL != db);
    assert(NULL != script);

    ok = false;
    do {
        struct pkg *pkg;
        struct pkgdb_it *it;
        const char *pkg_name;
        package_t *script_package;

        it = NULL;
        pkg = NULL;
        if (NULL == (it = pkgdb_query_which(pkg_db, script->path, false))) {
            set_generic_error(error, "failed to fetch package owner of %s", script->path);
            break;
        }
        if (EPKG_OK == pkgdb_it_next(it, &pkg, PKG_LOAD_FILES | PKG_LOAD_SHLIBS_REQUIRED)) {
#ifdef HAVE_PKG_SHLIBS_REQUIRED
            /* pkg < 1.18 */
            char *shlib_name;
#else
            /* pkg >= 1.18 */
            const char *shlib_name;
            struct pkg_stringlist *sl;
            struct pkg_stringlist_iterator *slit;
#endif /* pkg_shlibs_required */

            shlib_name = NULL;
            pkg_get_string(pkg, PKG_NAME, pkg_name);
            if (NULL == (script_package = package_retrieve(db, pkg_name, error))) {
                break;
            }
            if (!associate_package_to_script(db, script, script_package, error)) {
                break;
            }
            // IMPORTANT NOTE: from this point do NOT use pkg_name (will not be valid - freed - later), use script->package->name or script_package->name instead
#ifdef HAVE_PKG_SHLIBS_REQUIRED
            /* pkg < 1.18 */
            while (EPKG_OK == pkg_shlibs_required(pkg, &shlib_name)) {
#else
            /* pkg >= 1.18 */
            pkg_get_stringlist(pkg, PKG_SHLIBS_REQUIRED, sl); // WTF?!? pkg_attr vs pkg_list (enum): how PKG_SHLIBS_REQUIRED doesn't conflict with PKG_MESSAGE?!?
            slit = pkg_stringlist_iterator(sl);
            while (NULL != (shlib_name = pkg_stringlist_next(slit))) {
#endif /* pkg_shlibs_required */
                const char *name;
                struct pkgdb_it *it;

                if (NULL != (it = pkgdb_query_shlib_provide(pkg_db, shlib_name))) {
                    struct pkg *shlib_pkg;

                    shlib_pkg = NULL;
                    while (EPKG_OK == pkgdb_it_next(it, &shlib_pkg, PKG_LOAD_BASIC)) {
                        package_t *shlib_package;

                        pkg_get_string(shlib_pkg, PKG_NAME, name);
                        assert(NULL != name);
                        shlib_package = package_retrieve(db, name, error);
                        assert(NULL != shlib_package);
                        dlist_insert_unique(&shlib_package->rshlibs, (CmpFunc) compare_scripts, script);
                    }
                    pkgdb_it_free(it);
                }
            }
#ifndef HAVE_PKG_SHLIBS_REQUIRED
            /* pkg >= 1.18 */
            free(slit);
            free(sl);
#endif /* !pkg_shlibs_required */
            pkg_free(pkg);
        }
        if (NULL != it) {
            pkgdb_it_free(it);
        }
        ok = true;
    } while (false);

    return ok;
}

static rc_d_script_t *register_script(services_db_t *db, const char *name, const char *path)
{
    bool put;
    rc_d_script_t *script;

    assert(NULL != db);
    assert(NULL != path);

    script = malloc(sizeof(*script));
    script->package = NULL;
    script->path = strdup(path);
    script->name = script->path + strlen(script->path) - strlen(name);
    dlist_init(&script->befores, NULL, NULL);
    dlist_init(&script->parents, NULL, NULL);
    dlist_init(&script->requires, NULL, NULL);
    dlist_init(&script->keywords, NULL, NULL);
    dlist_init(&script->children, NULL, NULL);
    put = hashtable_put(&db->scripts, HT_PUT_ON_DUP_KEY_PRESERVE, script->name, script, NULL);
    assert(put);

    return script;
}

static bool scan_rc_d_directory(struct pkgdb *pkg_db, services_db_t *db, const char *directory, char **error)
{
    bool ok;
    DIR *dirp;

//     assert(NULL != pkg_db);
    assert(NULL != db);
    assert(NULL != directory);

    ok = true;
    if (NULL == (dirp = opendir(directory))) {
        ok = false;
        set_system_error(error, "opendir(3) %s failed", directory);
    } else {
        struct dirent *dp;

        while (NULL != (dp = readdir(dirp))) {
            struct stat sb;
            rc_d_script_t *script;
            char rc_d_script_path[MAXPATHLEN];

            if (0 == strcmp(".", dp->d_name) || 0 == strcmp("..", dp->d_name)) {
                continue;
            }
            if (!path_join(rc_d_script_path, rc_d_script_path + STR_SIZE(rc_d_script_path), error, directory, dp->d_name, NULL)) {
                ok = false;
                break;
            }
            if (0 != stat(rc_d_script_path, &sb)) {
                ok = false;
                set_errno_error(error, errno, "stat(2) failed for %s", rc_d_script_path);
                break;
            }
            if (!S_ISREG(sb.st_mode)) {
                continue;
            }
            script = register_script(db, dp->d_name, rc_d_script_path);
            if (NULL != pkg_db && !pkg_from_rc_d_script(pkg_db, db, script, error)) {
                ok = false;
                break;
            }
            if (!parse_rc_d_script(db, script, error)) {
                ok = false;
                break;
            }
        }
        closedir(dirp);
    }

    return ok;
}

void package_to_services_iterator(Iterator *it, services_db_t *db, const char *pkg_name)
{
    package_t *pkg;

    assert(NULL != db);
    assert(NULL != pkg_name);

    if (hashtable_get(&db->packages, pkg_name, &pkg)) {
        dlist_to_iterator(it, &pkg->scripts);
    } else {
        null_terminated_ptr_array_to_iterator(it, (void **) empty_iterator_data);
    }
}

void services_db_add_services_from_package_to_services_selection(services_db_t *db, services_selection_t *ss, const char *pkg_name, service_action_t action, bool include_rshlibs)
{
    Iterator it;
    package_t *pkg;

    assert(NULL != db);
    assert(NULL != ss);
    assert(NULL != pkg_name);

    if (hashtable_get(&db->packages, pkg_name, &pkg)) {
        rc_d_script_t *script;

        dlist_to_iterator(&it, &pkg->scripts);
        for (iterator_first(&it); iterator_is_valid(&it, NULL, &script); iterator_next(&it)) {
            services_selection_add_direct(ss, script->name, action);
//             debug("service %s needs to be stopped/restarted before deletion/upgrade of package %s", script->name, pkg_name);
        }
        iterator_close(&it);
    }
    if (include_rshlibs) {
        rc_d_script_t *script;

        services_db_rshlib(&it, db, pkg_name);
        for (iterator_first(&it); iterator_is_valid(&it, NULL, &script); iterator_next(&it)) {
            services_selection_add_rdep(ss, script->name, action);
        }
        iterator_close(&it);
    }
}

void services_db_rshlib(Iterator *it, services_db_t *db, const char *pkg_name)
{
    package_t *pkg;

    if (!hashtable_get(&db->packages, pkg_name, &pkg)) {
        null_terminated_ptr_array_to_iterator(it, (void **) empty_iterator_data);
    } else {
        dlist_to_iterator(it, &pkg->rshlibs);
    }
}

#if 0
const char *script_name(const rc_d_script_t *script)
{
    return script->name;
}

const char *script_path(const rc_d_script_t *script)
{
    return script->path;
}

const package_t *script_package(const rc_d_script_t *script)
{
    return script->package;
}

void script_befores(Iterator *it, const rc_d_script_t *script)
{
    dlist_to_iterator(it, &script->befores);
}

void script_requires(Iterator *it, const rc_d_script_t *script)
{
    dlist_to_iterator(it, &script->requires);
}

void script_keywords(Iterator *it, const rc_d_script_t *script)
{
    dlist_to_iterator(it, &script->keywords);
}
#endif

void script_get2(const rc_d_script_t *script, ...)
{
    int attr;
    va_list ap;

    va_start(ap, script);
    while (-1 != (attr = va_arg(ap, int))) {
        switch (attr) {
            case SCRIPT_ATTR_NAME:
                *va_arg(ap, const char **) = script->name;
                break;
            case SCRIPT_ATTR_PATH:
                *va_arg(ap, const char **) = script->path;
                break;
            case SCRIPT_ATTR_PACKAGE:
                *va_arg(ap, const package_t **) = script->package;
                break;
            case SCRIPT_ATTR_BEFORES:
                dlist_to_iterator(va_arg(ap, Iterator *), __DECONST(DList *, &script->befores));
                break;
            case SCRIPT_ATTR_REQUIRES:
                dlist_to_iterator(va_arg(ap, Iterator *), __DECONST(DList *, &script->requires));
                break;
            case SCRIPT_ATTR_KEYWORDS:
                dlist_to_iterator(va_arg(ap, Iterator *), __DECONST(DList *, &script->keywords));
                break;
            default:
                assert(false);
                break;
        }
    }
    va_end(ap);
}
