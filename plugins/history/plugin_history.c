#include <stdlib.h> /* atoi */
#include <sysexits.h> /* EX_USAGE */
#include <getopt.h>
#include <time.h>
#include <pkg.h>

#include "common.h"
#include "error/error.h"
#include "sqlite/sqlite.h"
#include "shared/os.h"
#include "shared/path_join.h"
#include "shared/argv.h"
#include "kissc/parsenum.h"
#include "kissc/stpcpy_sp.h"
#include "date.h"

static struct pkg_plugin *self;

static char DESCRIPTION[] = "Keep track of operations on packages";

#define TABLE_COMMANDS "history_commands"
#define TABLE_PACKAGES "history_lines"
#define TABLE_OPERATIONS "history_operations"

enum {
    STMT_CREATE_COMMAND,
    STMT_CREATE_LINE,
    STMT_LIST_LINE,
    STMT_SEARCH_LINE_EXACT,
    STMT_SEARCH_LINE_EXACT_CI,
    STMT_SEARCH_LINE_GLOB,
#ifdef WITH_REGEX
    STMT_SEARCH_LINE_REGEX,
#endif /* WITH_REGEX */
};

typedef struct {
    int limit;
    int statement;
    int operations;
    bool use_origin;
    time_t from, to;
} query_options_t;

#define STRINGIFY(s) #s
#define STRINGIFY_EXPAND(s) STRINGIFY(s)

#if 0
enum {
    PKG_SHIFT_OP_INSTALL,
    PKG_SHIFT_OP_DEINSTALL,
    PKG_SHIFT_OP_UPGRADE,
};
#else
// can't expand PKG_OP_INSTALL to 1<<0 with an enum
# define PKG_SHIFT_OP_INSTALL 0
# define PKG_SHIFT_OP_DEINSTALL 1
# define PKG_SHIFT_OP_UPGRADE 2
#endif

#define PKG_OP_INSTALL   (1<<PKG_SHIFT_OP_INSTALL)
#define PKG_OP_DEINSTALL (1<<PKG_SHIFT_OP_DEINSTALL)
#define PKG_OP_UPGRADE   (1<<PKG_SHIFT_OP_UPGRADE)
#define PKG_OP_REMOVE    PKG_OP_DEINSTALL
#define PKG_OP_ALL       (PKG_OP_INSTALL | PKG_OP_DEINSTALL | PKG_OP_UPGRADE)

#define STMT_SEARCH_LINE_BY_NAME(operator, after_placeholder) \
    DECL_STMT( \
        " SELECT c.inserted_at, c.command, l.name, l.origin, l.repo, l.old_version, l.new_version, l.operation_id" \
        " FROM " TABLE_COMMANDS " c JOIN " TABLE_PACKAGES " l ON c.id = l.command_id" \
        " WHERE l.name " operator " ? " after_placeholder \
        " AND (l.operation_id & ?) <> 0" \
        " AND (inserted_at BETWEEN ? AND ?)" \
        " ORDER BY c.inserted_at DESC" \
        " LIMIT ?", \
        "siiii", \
        "issssssi" \
    )

static sqlite_statement_t statements[] = {
    [ STMT_CREATE_COMMAND ] = DECL_STMT(
        "INSERT INTO " TABLE_COMMANDS "(inserted_at, command) VALUES(strftime('%s', 'now'), ?)",
        "s",
        ""
    ),
    [ STMT_CREATE_LINE ] = DECL_STMT(
        "INSERT INTO " TABLE_PACKAGES "(repo, name, origin, old_version, new_version, operation_id, command_id) VALUES(?, ?, ?, ?, ?, ?, ?)",
        "sssssii",
        ""
    ),
    [ STMT_LIST_LINE ] = DECL_STMT(
        " SELECT c.id, c.inserted_at, c.command, l.name, l.origin, l.repo, l.old_version, l.new_version, l.operation_id"
        " FROM " TABLE_COMMANDS " c"
        " LEFT JOIN " TABLE_PACKAGES " l ON c.id = l.command_id"
        " WHERE (c.inserted_at BETWEEN ? AND ?) AND (l.operation_id & ?) <> 0"
        " ORDER BY c.inserted_at DESC, l.name"
        " LIMIT ?",
        "ttii",
        /* c */ "its" /* l */ "sssssi"
    ),
    [ STMT_SEARCH_LINE_EXACT ] = STMT_SEARCH_LINE_BY_NAME("=", ""),
    [ STMT_SEARCH_LINE_EXACT_CI ] = STMT_SEARCH_LINE_BY_NAME("=", "COLLATE NOCASE"),
    [ STMT_SEARCH_LINE_GLOB ] = STMT_SEARCH_LINE_BY_NAME("GLOB", ""),
#ifdef WITH_REGEX
    [ STMT_SEARCH_LINE_REGEX ] = STMT_SEARCH_LINE_BY_NAME("REGEXP"),
#endif /* WITH_REGEX */
};

static pkg_error_t db_open(sqlite_db_t **db, int mode, char **error)
{
    pkg_error_t status;

    status = EPKG_FATAL;
    do {
        char dbpath[MAXPATHLEN];

        if (!path_join(dbpath, dbpath + STR_SIZE(dbpath), error, pkg_dbdir(), "history.sqlite", NULL)) {
            break;
        }
        status = sqlite_open(dbpath, mode, db, error);
        if (EPKG_OK == status) {
            status = EPKG_FATAL;
            if (!sqlite_create_or_migrate(*db, TABLE_COMMANDS, "CREATE TABLE " TABLE_COMMANDS "(\n\
                id INTEGER NOT NULL PRIMARY KEY,\n\
                inserted_at INT NOT NULL,\n\
                command TEXT NOT NULL\n\
            );\n\
            CREATE INDEX " TABLE_COMMANDS "_inserted_at ON " TABLE_COMMANDS "(inserted_at);", NULL, 0, error)) {
                break;
            }
            if (!sqlite_create_or_migrate(*db, TABLE_OPERATIONS, "CREATE TABLE " TABLE_OPERATIONS "(\n\
                id INTEGER NOT NULL,\n\
                name TEXT NOT NULL,\n\
                PRIMARY KEY(id)\n\
            );\n\
            INSERT INTO " TABLE_OPERATIONS "(id, name) VALUES(" STRINGIFY_EXPAND(PKG_OP_INSTALL) ", 'install');\n\
            INSERT INTO " TABLE_OPERATIONS "(id, name) VALUES(" STRINGIFY_EXPAND(PKG_OP_DEINSTALL) ", 'deinstall');\n\
            INSERT INTO " TABLE_OPERATIONS "(id, name) VALUES(" STRINGIFY_EXPAND(PKG_OP_UPGRADE) ", 'upgrade');", NULL, 0, error)) {
                break;
            }
            if (!sqlite_create_or_migrate(*db, TABLE_PACKAGES, "CREATE TABLE " TABLE_PACKAGES "(\n\
                id INTEGER NOT NULL PRIMARY KEY,\n\
                -- NOTE: repo is NULL on deletion\n\
                repo TEXT NULL,\n\
                name TEXT NOT NULL,\n\
                origin TEXT NOT NULL,\n\
                old_version TEXT NULL,\n\
                new_version TEXT NOT NULL,\n\
                command_id INT NOT NULL REFERENCES " TABLE_COMMANDS "(id) ON UPDATE CASCADE ON DELETE CASCADE,\n\
                operation_id INT NOT NULL REFERENCES " TABLE_OPERATIONS "(id) ON UPDATE CASCADE ON DELETE CASCADE\n\
            );\n\
            CREATE INDEX " TABLE_PACKAGES "_command_id_index ON " TABLE_PACKAGES "(command_id);\n\
            CREATE INDEX " TABLE_PACKAGES "_operation_id_index ON " TABLE_PACKAGES "(operation_id);", NULL, 0, error)) {
                break;
            }
            if (HAS_FLAG(PKGDB_MODE_WRITE, mode) && !sqlite_set_user_version(*db, HISTORY_VERSION_NUMBER, error)) {
                break;
            }
        } else if (EPKG_ENODB == status) {
            pkg_plugin_info(self, "the database used by plugin %s does not yet exist and can only be initialized by root", NAME);
            break;
        } else {
            break;
        }
        if (!sqlite_stmt_prepare(*db, statements, ARRAY_SIZE(statements), error)) {
            break;
        }
        status = EPKG_OK;
    } while (false);

    return status;
}

static int handle_hooks(void *, struct pkgdb *);

char *timestamp_to_localtime(time_t t, const char *format, char *buffer, const char * const buffer_end, char **error)
{
    char *w;
    size_t written;
    struct tm ltm = { 0 };

    w = NULL;
    do {
        if (NULL == localtime_r(&t, &ltm)) {
            set_generic_error(error, "localtime_r(3) failed");
            break;
        }
        if (0 == (written = strftime(buffer, buffer_end - buffer, NULL == format ? "%x %X" : format, &ltm))) {
            set_generic_error(error, "strftime(3) failed");
            break;
        }
        w = buffer + written;
    } while (false);

    return w;
}

static const char *operation_names[] = {
    [PKG_SHIFT_OP_INSTALL] = "Installed",
    [PKG_SHIFT_OP_DEINSTALL] = "Deleted",
    [PKG_SHIFT_OP_UPGRADE] = "Upgraded",
};

static const char *operation_name(int operation)
{
    size_t i;
    const char *name;

    name = "???";
    for (i = 0; i < ARRAY_SIZE(operation_names); i++) {
        if (operation == (1<<i)) {
            name = operation_names[i];
            break;
        }
    }

    return name;
}

static void display_command(time_t inserted_at, const char *command)
{
    char datetime[STR_SIZE("dd/mm/YYYY HH:ii:ss")];

    timestamp_to_localtime(inserted_at, NULL, datetime, datetime + STR_SIZE(datetime), NULL);
    printf("On %s: %s\n", datetime, command);
}

// #define REPO_PADDING_LEN      -20
#define VERSION_PADDING_LEN   -20
#define PACKAGE_PADDING_LEN   -40
#define OPERATION_PADDING_LEN -20

static void display_package_header(void)
{
    printf(
        "\t%*s %*s %*s %*s %s\n",
        OPERATION_PADDING_LEN, "Operation",
        PACKAGE_PADDING_LEN, "Package",
        VERSION_PADDING_LEN, "New version",
        VERSION_PADDING_LEN, "Old version",
        /*REPO_PADDING_LEN, */"Repository"
    );
}

static void display_package(int operation, const char *name, const char *repo, const char *new_version, const char *old_version)
{
    printf(
        "\t%*s %*s %*s %*s %s\n",
        OPERATION_PADDING_LEN, operation_name(operation),
        PACKAGE_PADDING_LEN, name,
        VERSION_PADDING_LEN, new_version,
        VERSION_PADDING_LEN, NULL == old_version ? "-" : old_version,
        /*REPO_PADDING_LEN, */NULL == repo ? "-" : repo
    );
}

static void display_history_full(const query_options_t *qo)
{
    Iterator it;
    time_t inserted_at;
    int operation, command_id, previous_command_id;
    char *command, *name, *repo, *old_version, *new_version;

    previous_command_id = -1;
    statement_bind(&statements[STMT_LIST_LINE], qo->from, qo->to, qo->operations, qo->limit);
    statement_to_iterator(&it, &statements[STMT_LIST_LINE], &command_id, &inserted_at, &command, qo->use_origin ? NULL : &name, qo->use_origin ? &name : NULL, &repo, &old_version, &new_version, &operation);
    iterator_first(&it);
    if (iterator_is_valid(&it, NULL, NULL)) {
        do {
            if (previous_command_id != command_id) {
                if (-1 != previous_command_id) {
                    fputc('\n', stdout);
                }
                display_command(inserted_at, command);
                display_package_header();
            }
            if (NULL == name) {
                printf("no operation\n");
            } else {
                display_package(operation, name, repo, new_version, old_version);
            }
            previous_command_id = command_id;
            iterator_next(&it);
        } while (iterator_is_valid(&it, NULL, NULL));
    } else {
        printf("nothing to show\n");
    }
    iterator_close(&it);
}

static void display_history_search(const query_options_t *qo, const char *searched)
{
    Iterator it;
    int operation;
    time_t inserted_at;
    char *command, *name, *repo, *old_version, *new_version;

    statement_bind(&statements[qo->statement], searched, qo->operations, qo->from, qo->to, qo->limit);
    statement_to_iterator(&it, &statements[qo->statement], &inserted_at, &command, qo->use_origin ? NULL : &name, qo->use_origin ? &name : NULL, &repo, &old_version, &new_version, &operation);
    iterator_first(&it);
    if (iterator_is_valid(&it, NULL, NULL)) {
        do {
            display_command(inserted_at, command);
            display_package_header();
            display_package(operation, name, repo, new_version, old_version);
            printf("\n");
            iterator_next(&it);
        } while (iterator_is_valid(&it, NULL, NULL));
    } else {
        printf("nothing to show\n");
    }
    iterator_close(&it);
}

static void query_options_init(query_options_t *qo)
{
    qo->limit = 100;
    qo->operations = 0;
    qo->to = time(NULL);
    qo->from = (time_t) 0;
    qo->use_origin = false;
    qo->statement = STMT_SEARCH_LINE_EXACT_CI;
}

static char optstr[] = "Cdf:gin:out:";

static struct option long_options[] = {
    { "glob",             no_argument,       NULL, 'g' },
    { "case-sensitive",   no_argument,       NULL, 'C' },
#ifdef WITH_REGEX
    { "regex",            no_argument,       NULL, 'x' },
#endif /* WITH_REGEX */
    { "delete",           no_argument,       NULL, 'd' },
    { "install",          no_argument,       NULL, 'i' },
    { "upgrade",          no_argument,       NULL, 'u' },
    { "limit",            required_argument, NULL, 'n' },
    { "from",             required_argument, NULL, 'f' },
    { "to",               required_argument, NULL, 't' },
    { NULL,               no_argument,       NULL, 0   },
};

static void usage(void)
{
    fputs("usage: pkg history [-Cgdiu] [-n count] [-f date] [-t date] [package]\n", stderr);
    fputs("-C, --case-sensitive\n", stderr);
    fputs("\tmatching case sensitively against *package* (default is to ignore case except for -g/--glob)\n", stderr);
    fputs("-g, --glob\n", stderr);
    fputs("\ttreat *package* as a shell glob pattern\n", stderr);
#ifdef WITH_REGEX
    fputs("-x, --regex\n", stderr);
    fputs("\ttreat *package* as a regular expression\n", stderr);
#endif /* WITH_REGEX */
    fputs("-d, --delete\n", stderr);
    fputs("\tdon't show the full history, only include package deletions\n", stderr);
    fputs("-i, --insert\n", stderr);
    fputs("\tdon't show the full history, only include package installations\n", stderr);
    fputs("-u, --upgrade\n", stderr);
    fputs("\tdon't show the full history, only include package upgrades\n", stderr);
    fputs("-n *count*, --limit=*count*\n", stderr);
    fputs("\tdisplay at most *count* pkg operations\n", stderr);
    fputs("-f *date*, --from=*date*\n", stderr);
    fputs("\tthe search begins from *date*\n", stderr);
    fputs("-t *date*, --to=date\n", stderr);
    fputs("\tthe search ends at *date*\n", stderr);
}

static int pkg_history_main(int argc, char **argv)
{
    int ch;
    char *error;
    sqlite_db_t *db;
    pkg_error_t status;
    query_options_t qo;

    db = NULL;
    error = NULL;
    status = EPKG_FATAL;
    query_options_init(&qo);
    while (-1 != (ch = getopt_long(argc, argv, optstr, long_options, NULL))) {
        switch (ch) {
            case 'd':
                qo.operations |= PKG_OP_DEINSTALL;
                break;
            case 'i':
                qo.operations |= PKG_OP_INSTALL;
                break;
            case 'u':
                qo.operations |= PKG_OP_UPGRADE;
                break;
            /* last of C/g/x option wins */
            case 'C':
                qo.statement = STMT_SEARCH_LINE_EXACT;
                break;
            case 'g':
                qo.statement = STMT_SEARCH_LINE_GLOB;
                break;
#ifdef WITH_REGEX
            case 'x':
                qo.statement = STMT_SEARCH_LINE_REGEX;
                break;
#endif /* WITH_REGEX */
            case 'o':
                qo.use_origin = true;
                break;
            case 'n':
            {
                int32_t min, max, val;

                min = 1;
                max = INT_MAX;
                if (PARSE_NUM_NO_ERR != strtoint32_t((const char *) optarg, NULL, 10, &min, &max, &val)) {
                    set_generic_error(&error, "parameter --count/-n is invalid: integer expected in range of [1;%d]", INT_MAX);
                    goto invalid_argument;
                }
                qo.limit = (int) val;
                break;
            }
            case 'f':
                if (!parse_date(optarg, &qo.from, &error)) {
                    goto invalid_argument;
                }
                break;
            case 't':
                if (!parse_date(optarg, &qo.to, &error)) {
                    goto invalid_argument;
                }
                break;
            default:
                usage();
//                 status = EX_USAGE;
                return EX_USAGE;
        }
    }
    argc -= optind;
    argv += optind;

    if (0 == qo.operations) {
        qo.operations = PKG_OP_ALL;
    }
    do {
        if (EPKG_OK != db_open(&db, PKGDB_MODE_READ, &error)) {
            break;
        }
        if (0 == argc) {
            display_history_full(&qo);
#if 1
        } else if (1 == argc) {
            display_history_search(&qo, argv[0]);
#else
        } else {
            int i;

            for (i = 0; i < argc; i++) {
                display_history_search(&qo, argv[i]);
            }
#endif
        }
        status = EPKG_OK;
    } while (false);
    if (NULL != db) {
        sqlite_close(db);
    }
invalid_argument: // TODO: kill me
    if (/*EPKG_OK != status && */NULL != error) {
        pkg_plugin_error(self, "%s", error);
        error_free(&error);
    }

    return EPKG_OK;
}

static int handle_hooks(void *data, struct pkgdb *UNUSED(_db))
{
    char *error;
    sqlite_db_t *db;
    pkg_error_t status;

    db = NULL;
    error = NULL;
    status = EPKG_FATAL;
    do {
        void *iter;
        char cmd[ARG_MAX];
        pkg_jobs_t job_type;
        char **args;
        struct pkg_jobs *jobs;
        struct pkg *new_pkg, *old_pkg;
        int command_id, operation, solved_type, fetch_status;

        iter = NULL;
        fetch_status = 0;
        jobs = (struct pkg_jobs *) data;
#if 0
        // record the run of pkg even if it does nothing? (we are in POST so the hook might not even run)
        if (pkg_jobs_count(jobs) < 1) {
            status = EPKG_OK;
            break;
        }
#endif
        job_type = pkg_jobs_type(jobs);
        if (EPKG_OK != db_open(&db, PKGDB_MODE_READ | PKGDB_MODE_WRITE, &error)) {
            break;
        }
        if (NULL == (args = get_pkg_cmd_line(STR_SIZE(cmd), NULL, &error))) {
            break;
        }
        if (!argv_join((const char **) args, cmd, cmd + STR_SIZE(cmd), &error)) {
            break;
        }
        if (!sqlite_transaction_begin(db, &error)) {
            break;
        }
        statement_bind(&statements[STMT_CREATE_COMMAND], cmd);
        statement_fetch(db, &statements[STMT_CREATE_COMMAND], &error);
        command_id = sqlite_last_insert_id(db);
        while (pkg_jobs_iter(jobs, &iter, &new_pkg, &old_pkg, &solved_type)) {
            char *name, *origin, *new_version, *old_version, *repo;

            pkg_get(new_pkg,
                PKG_NAME, &name,
                PKG_ORIGIN, &origin,
                PKG_VERSION, &new_version,
                PKG_OLD_VERSION, &old_version,
                PKG_REPONAME, &repo
            );
            switch (job_type) { // TODO: plutôt considérer solved_type ?
                case PKG_JOBS_INSTALL:
                    operation = PKG_OP_INSTALL;
                    break;
                case PKG_JOBS_DEINSTALL:
                case PKG_JOBS_AUTOREMOVE:
                    operation = PKG_OP_DEINSTALL;
                    break;
                case PKG_JOBS_UPGRADE:
                    if (PKG_SOLVED_INSTALL == solved_type) {
                        operation = PKG_OP_INSTALL;
                    } else {
                        operation = PKG_OP_UPGRADE;
                    }
                    break;
                case PKG_JOBS_FETCH:
                default:
                    assert(false);
                    break;
            }
            statement_bind(&statements[STMT_CREATE_LINE], repo, name, origin, old_version, new_version, operation, command_id);
            if (-1 == (fetch_status = statement_fetch(db, &statements[STMT_CREATE_LINE], &error))) {
                break;
            }
        }
        if (0 != fetch_status) {
            break;
        }
        if (!sqlite_transaction_commit(db, &error)) {
            break;
        }
        status = EPKG_OK;
    } while (false);
    if (NULL != db) {
        sqlite_close(db);
    }
    if (/*EPKG_OK != status && */NULL != error) {
        pkg_plugin_error(self, "%s", error);
        error_free(&error);
    }

    return status;
}

#define H(value) \
    {value, #value}

static const struct {
    pkg_plugin_hook_t value;
    const char *name;
} hooks[] = {
//     H(PKG_PLUGIN_HOOK_PRE_INSTALL),
    H(PKG_PLUGIN_HOOK_POST_INSTALL),
//     H(PKG_PLUGIN_HOOK_PRE_DEINSTALL),
    H(PKG_PLUGIN_HOOK_POST_DEINSTALL),
//     H(PKG_PLUGIN_HOOK_PRE_FETCH),
//     H(PKG_PLUGIN_HOOK_POST_FETCH),
//     H(PKG_PLUGIN_HOOK_EVENT),
//     H(PKG_PLUGIN_HOOK_PRE_UPGRADE),
    H(PKG_PLUGIN_HOOK_POST_UPGRADE),
//     H(PKG_PLUGIN_HOOK_PRE_AUTOREMOVE),
    H(PKG_PLUGIN_HOOK_POST_AUTOREMOVE),
//     PKG_PLUGIN_HOOK_PKGDB_CLOSE_RW,
};

#undef H

int pkg_plugin_init(struct pkg_plugin *p)
{
    size_t i;

    self = p;

    pkg_plugin_set(p, PKG_PLUGIN_NAME, NAME);
    pkg_plugin_set(p, PKG_PLUGIN_DESC, DESCRIPTION);
    pkg_plugin_set(p, PKG_PLUGIN_VERSION, HISTORY_VERSION_STRING);

    for (i = 0; i < ARRAY_SIZE(hooks); i++) {
        if (EPKG_OK != pkg_plugin_hook_register(p, hooks[i].value, handle_hooks)) {
            pkg_plugin_error(p, "failed to hook %s (%d)", hooks[i].name, hooks[i].value);
            return EPKG_FATAL;
        }
    }

    return EPKG_OK;
}

int pkg_register_cmd_count(void)
{
    return 1;
}

int pkg_register_cmd(int i, const char **name, const char **desc, int (**exec)(int argc, char **argv))
{
    assert(0 == i);

    *name = NAME;
    *desc = DESCRIPTION;
    *exec = pkg_history_main;

    return EPKG_OK;
}

int pkg_plugin_shutdown(struct pkg_plugin *UNUSED(p))
{
    return EPKG_OK;
}
