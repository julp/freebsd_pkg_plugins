#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/stat.h> /* stat */
#include <unistd.h> /* geteuid */

#include <pkg.h>
#include <sqlite3.h>

#include "common.h"
#include "error.h"
#include "sqlite.h"
#include "iterator.h"
#include "shared/os.h"

/**
 * NOTE:
 * - for sqlite3_column_* functions, the first column is 0
 * - in the other hand, for sqlite3_bind_* functions, the first parameter is 1
 */

#define set_sqlite_exec_error(error, errmsg, stmt) \
    set_generic_error(error, "[%s:%d] %s for %s", __func__, __LINE__, errmsg, stmt)

#define set_sqlite_stmt_error(error, db, stmt) \
    set_generic_error(error, "%s for %s", sqlite3_errmsg(db), (stmt)->statement)

struct sqlite_db_t {
    sqlite3 *db;
    user_version_t user_version;
};

typedef enum {
    SQLITE_TYPE_NULL,
    SQLITE_TYPE_BOOL,
    SQLITE_TYPE_BOOLEAN = SQLITE_TYPE_BOOL,
    SQLITE_TYPE_INT,
    SQLITE_TYPE_INT64,
    SQLITE_TYPE_TIME,
    SQLITE_TYPE_STRING,
    SQLITE_TYPE_IGNORE,
} sqlite_bind_type_t;

typedef struct {
    sqlite_bind_type_t type;
    void *ptr;
} sqlite_statement_bind_t;

typedef struct {
    /**
     * Keep result of last sqlite3_step call
     */
    int ret;
    sqlite3_stmt *stmt;
//     sqlite_statement_t *decl;
    size_t /*input_binds_count, */output_binds_count;
    sqlite_statement_bind_t /**input_binds, */*output_binds;
} sqlite_statement_state_t;

enum {
    STMT_GET_USER_VERSION,
//     STMT_SET_USER_VERSION,
    STMT_COUNT,
};

static sqlite_statement_t statements[STMT_COUNT] = {
    [ STMT_GET_USER_VERSION ] = DECL_STMT("PRAGMA user_version", "", ""),
//     [ STMT_SET_USER_VERSION ] = DECL_STMT("PRAGMA user_version = " STRINGIFY_EXPANDED(OVH_CLI_VERSION_NUMBER), "", ""), // PRAGMA doesn't permit parameter
};

#define VOIDP_TO_X(/*void **/ ptr, output_type) \
    *((output_type *) ((char *) ptr))

static void bool_input_bind(sqlite3_stmt *stmt, int no, va_list *ap)
{
    sqlite3_bind_int(stmt, no, va_arg(*ap, int));
}

static void bool_output_bind(sqlite3_stmt *stmt, int no, void *ptr, bool UNUSED(copy))
{
    VOIDP_TO_X(ptr, bool) = /*!!*/sqlite3_column_int(stmt, no);
}

static void int_input_bind(sqlite3_stmt *stmt, int no, va_list *ap)
{
    sqlite3_bind_int(stmt, no, va_arg(*ap, int));
}

static void int_output_bind(sqlite3_stmt *stmt, int no, void *ptr, bool UNUSED(copy))
{
    VOIDP_TO_X(ptr, int) = sqlite3_column_int(stmt, no);
}

static void int64_input_bind(sqlite3_stmt *stmt, int no, va_list *ap)
{
    sqlite3_bind_int64(stmt, no, va_arg(*ap, int64_t));
}

static void int64_output_bind(sqlite3_stmt *stmt, int no, void *ptr, bool UNUSED(copy))
{
    VOIDP_TO_X(ptr, int64_t) = sqlite3_column_int64(stmt, no);
}

static void time_t_input_bind(sqlite3_stmt *stmt, int no, va_list *ap)
{
    sqlite3_bind_int64(stmt, no, (int64_t) va_arg(*ap, time_t));
}

static void time_t_output_bind(sqlite3_stmt *stmt, int no, void *ptr, bool UNUSED(copy))
{
    VOIDP_TO_X(ptr, time_t) = sqlite3_column_int64(stmt, no);
}

#if 0
static void ignore_input_bind(sqlite3_stmt *UNUSED(stmt), int UNUSED(no), va_list *ap)
{
    // NOP
}
#endif

static void ignore_output_bind(sqlite3_stmt *UNUSED(stmt), int UNUSED(no), void *UNUSED(ptr), bool UNUSED(copy))
{
    // NOP
}

#if 0
static void time_t_intput_bind(sqlite3_stmt *stmt, int no, void *ptr)
{
    sqlite3_bind_int64(stmt, no, VOIDP_TO_X(ptr, time_t));
}

static void time_t_output_bind(sqlite3_stmt *stmt, int no, void *ptr, bool UNUSED(copy))
{
    VOIDP_TO_X(ptr, time_t) = sqlite3_column_int64(stmt, no);
}
#endif

static void string_intput_bind(sqlite3_stmt *stmt, int no, va_list *ap)
{
    sqlite3_bind_text(stmt, no, va_arg(*ap, char *), -1, SQLITE_TRANSIENT);
}

static void string_output_bind(sqlite3_stmt *stmt, int no, void *ptr, bool copy)
{
    char *uv;
    const unsigned char *sv;

    sv = sqlite3_column_text(stmt, no);
    if (NULL == sv) {
        uv = NULL;
    } else {
        uv = copy ? strdup((char *) sv) : (char *) sv;
    }
    VOIDP_TO_X(ptr, char *) = uv;
}

typedef enum {
    SQLITE_ID_BOOL    = (uint8_t) 'b',
    SQLITE_ID_BOOLEAN = SQLITE_TYPE_BOOL,
    SQLITE_ID_INT     = (uint8_t) 'i',
    SQLITE_ID_INT64   = (uint8_t) 'I',
    SQLITE_ID_TIME    = (uint8_t) 't',
    SQLITE_ID_STRING  = (uint8_t) 's',
    SQLITE_ID_IGNORE  = (uint8_t) '-',
} sqlite_id_type_t;

typedef struct {
    sqlite_id_type_t id;
//     const char *c_type;
//     size_t c_type_len;
//     const char *sqlite_type;
//     size_t sqlite_type_len;
    sqlite_bind_type_t bind_type;
    void (*set_input_bind)(sqlite3_stmt *, int, va_list *);
    void (*set_output_bind)(sqlite3_stmt *, int, void *, bool);
} sqlite_type_callback_t;

static sqlite_type_callback_t sqlite_type_callbacks[] = {
    [ SQLITE_TYPE_BOOL ]   = { SQLITE_ID_BOOL, SQLITE_TYPE_BOOL, bool_input_bind, bool_output_bind, },
    [ SQLITE_TYPE_INT ]    = { SQLITE_ID_INT, SQLITE_TYPE_INT, int_input_bind, int_output_bind, },
    [ SQLITE_TYPE_INT64 ]  = { SQLITE_ID_INT, SQLITE_TYPE_INT, int64_input_bind, int64_output_bind, },
    [ SQLITE_TYPE_TIME ]   = { SQLITE_ID_TIME, SQLITE_TYPE_TIME, time_t_input_bind, time_t_output_bind, },
    [ SQLITE_TYPE_STRING ] = { SQLITE_ID_STRING, SQLITE_TYPE_STRING, string_intput_bind, string_output_bind, },
    [ SQLITE_TYPE_IGNORE ] = { SQLITE_ID_IGNORE, SQLITE_TYPE_IGNORE, NULL, ignore_output_bind, },
};

static const sqlite_type_callback_t *sqlite_type_callbacks2[256] = {
    [ SQLITE_ID_BOOL ]   = &sqlite_type_callbacks[SQLITE_TYPE_BOOL],
    [ SQLITE_ID_INT ]    = &sqlite_type_callbacks[SQLITE_TYPE_INT],
    [ SQLITE_ID_INT64 ]  = &sqlite_type_callbacks[SQLITE_TYPE_INT64],
    [ SQLITE_ID_TIME ]   = &sqlite_type_callbacks[SQLITE_TYPE_TIME],
    [ SQLITE_ID_STRING ] = &sqlite_type_callbacks[SQLITE_TYPE_STRING],
    [ SQLITE_ID_IGNORE ] = &sqlite_type_callbacks[SQLITE_TYPE_IGNORE],
};

static bool statement_iterator_is_valid(const void *UNUSED(collection), void **state)
{
    sqlite_statement_state_t *sss;

    assert(NULL != state);
    sss = *(sqlite_statement_state_t **) state;

    return SQLITE_ROW == sss->ret;
}

static void statement_iterator_first(const void *collection, void **state)
{
    sqlite_statement_t *stmt;
    sqlite_statement_state_t *sss;

    assert(NULL != state);
    assert(NULL != collection);

    stmt = (sqlite_statement_t *) collection;
    sss = *(sqlite_statement_state_t **) state;
    sss->ret = sqlite3_step(stmt->prepared);
}

static void statement_iterator_next(const void *collection, void **state)
{
    sqlite_statement_t *stmt;
    sqlite_statement_state_t *sss;

    assert(NULL != state);
    assert(NULL != collection);

    stmt = (sqlite_statement_t *) collection;
    sss = *(sqlite_statement_state_t **) state;
    sss->ret = sqlite3_step(stmt->prepared);
}

static void statement_iterator_current(const void *collection, void **state, void **UNUSED(value), void **UNUSED(key))
{
    sqlite_statement_t *stmt;
    sqlite_statement_state_t *sss;

    assert(NULL != state);
    assert(NULL != collection);

    stmt = (sqlite_statement_t *) collection;
    sss = *(sqlite_statement_state_t **) state;
    if (0 != sss->output_binds_count) {
        size_t i;

        for (i = 0; i < sss->output_binds_count; i++) {
            sqlite_type_callbacks[sss->output_binds[i].type].set_output_bind(stmt->prepared, i, sss->output_binds[i].ptr, false);
        }
    }
}

static void statement_iterator_close(void *state)
{
    sqlite_statement_state_t *sss;

    assert(NULL != state);

    sss = (sqlite_statement_state_t *) state;
    if (0 != sss->output_binds_count) {
        free(sss->output_binds);
    }
    free(sss);
}

void statement_to_iterator(Iterator *it, sqlite_statement_t *stmt, ...)
{
    sqlite_statement_state_t *sss;

    sss = malloc(sizeof(*sss));
    assert(NULL != sss);
    sss->output_binds_count = strlen(stmt->output_binds);
    if (0 == sss->output_binds_count) {
        sss->output_binds = NULL;
    } else {
        size_t i;
        va_list ap;

        va_start(ap, stmt);
        sss->output_binds = calloc(sss->output_binds_count, sizeof(*sss->output_binds));
        for (i = 0; i < sss->output_binds_count; i++) {
            const sqlite_type_callback_t *sqlite_type_callback;

            assert(i <= sss->output_binds_count);
            sqlite_type_callback = sqlite_type_callbacks2[(uint8_t) stmt->output_binds[i]];
            assert(NULL != sqlite_type_callback);
            sss->output_binds[i].ptr = va_arg(ap, void *);
            sss->output_binds[i].type = NULL == sss->output_binds[i].ptr ? SQLITE_TYPE_IGNORE : sqlite_type_callback->bind_type;
        }
        va_end(ap);
    }
    iterator_init(
        it, stmt, sss,
        statement_iterator_first, NULL,
        statement_iterator_current,
        statement_iterator_next, NULL,
        statement_iterator_is_valid,
        statement_iterator_close,
        NULL, NULL, NULL
    );
}

void statement_bind(sqlite_statement_t *stmt, ...)
{
    va_list ap;
    const char *p;

    sqlite3_reset(stmt->prepared);
    sqlite3_clear_bindings(stmt->prepared);
    assert(strlen(stmt->input_binds) == ((size_t) sqlite3_bind_parameter_count(stmt->prepared)));

    va_start(ap, stmt);
    for (p = stmt->input_binds; '\0' != *p; p++) {
        const sqlite_type_callback_t *sqlite_type_callback;

        sqlite_type_callback = sqlite_type_callbacks2[(uint8_t) *p];
        assert(NULL != sqlite_type_callback);
        assert(NULL != sqlite_type_callback->set_input_bind);
        sqlite_type_callback->set_input_bind(stmt->prepared, p - stmt->input_binds + 1, &ap);
    }
    va_end(ap);
}

/**
 * returns
 * + -1 on error
 * + 0 if there was no row to fetch
 * + 1 if a row was fetched
 */
int statement_fetch(sqlite_db_t *dbh, sqlite_statement_t *stmt, char **error, ...)
{
    int ret;
    va_list ap;

    ret = -1;
    va_start(ap, error);
    switch (sqlite3_step(stmt->prepared)) {
        case SQLITE_ROW:
        {
            const char *p;

            assert(((size_t) sqlite3_column_count(stmt->prepared)) >= strlen(stmt->output_binds)); // allow unused result columns at the end
            for (p = stmt->output_binds; '\0' != *p; p++) {
                void *ptr;
                const sqlite_type_callback_t *sqlite_type_callback;

                ptr = va_arg(ap, void *);
                sqlite_type_callback = sqlite_type_callbacks2[(uint8_t) *p];
                assert(NULL != sqlite_type_callback);
                assert(NULL != sqlite_type_callback->set_output_bind);
                sqlite_type_callback->set_output_bind(stmt->prepared, p - stmt->output_binds, ptr, false);
            }
            ret = 1;
            break;
        }
        case SQLITE_DONE:
            // empty result set
            ret = 0;
            break;
        default:
//             error_set(error, "%s for %s", sqlite3_errmsg(), sqlite3_sql(stmt->prepared));
            set_sqlite_stmt_error(error, dbh->db, stmt);
            break;
    }
    va_end(ap);

    return ret;
}

static pkg_error_t check_db_file(const char *path, char **error)
{
    struct stat sb;

    if (0 == stat(path, &sb)) {
        return EPKG_OK;
    } else if (ENOENT == errno) {
        return EPKG_ENODB;
    } else {
        set_errno_error(error, errno, "stat(2) failed for %s", path);
        return EPKG_FATAL;
    }
}

static bool sqlite_execf(sqlite3 *db, char **error, const char *query, ...)
{
    int retval;
    va_list ap;
    char *buffer, *errmsg;

    buffer = NULL;
    va_start(ap, query);
    buffer = sqlite3_vmprintf(query, ap);
    assert(NULL != buffer); // malloc failed
    va_end(ap);
    if (SQLITE_OK != (retval = sqlite3_exec(db, buffer, NULL, NULL, &errmsg))) {
        set_sqlite_exec_error(error, errmsg, buffer);
        sqlite3_free(errmsg);
    }
    if (NULL != buffer) {
        sqlite3_free(buffer);
    }

    return SQLITE_OK == retval;
}

bool sqlite_set_user_version(sqlite_db_t *dbh, user_version_t user_version, char **error)
{
    return sqlite_execf(dbh->db, error, "PRAGMA user_version = %" PRId64 ";", user_version);
}

static int sqlite_trace_callback(unsigned UNUSED(trace), void *UNUSED(context), void *p, void *UNUSED(x))
{
    char *query;
    sqlite3_stmt *stmt;

    stmt = (sqlite3_stmt *) p;
    query = sqlite3_expanded_sql(stmt);
    fprintf(stderr, "[TRACE] %s\n", query);
    sqlite3_free(query);

    return 0;
}

/**
 * NOTE:
 * - mode is PKGDB_MODE_READ and/or PKGDB_MODE_WRITE
 * - Possible returned values are:
 *   + EPKG_ENODB if database doesn't exist but current user can't create it
 *   + EPKG_OK on success
 *   + EPKG_FATAL on error
 */
pkg_error_t sqlite_open(const char *path, int mode, sqlite_db_t **dbh, char **error)
{
    pkg_error_t status;

    do {
        int flags;
        sqlite_db_t *tmp;
        pkg_error_t db_state;

        tmp = NULL;
        status = EPKG_FATAL;
        flags = SQLITE_OPEN_READONLY;
        if (EPKG_FATAL == (db_state = check_db_file(path, error))) {
            break;
        }
        if (0 == geteuid()) {
            if (EPKG_ENODB == db_state) {
                flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
            } else {
                flags = HAS_FLAG(mode, PKGDB_MODE_WRITE) ? SQLITE_OPEN_READWRITE : SQLITE_OPEN_READONLY;
            }
        } else if (EPKG_ENODB == db_state) {
            status = db_state;
            break;
        }
        if (NULL == (tmp = malloc(sizeof(*tmp)))) {
            set_malloc_error(error, sizeof(*tmp));
            break;
        }
        tmp->db = NULL;
        tmp->user_version = -1;
        if (SQLITE_OK != sqlite3_initialize()) {
            set_generic_error(error, "can't initialize sqlite");
            break;
        }
        if (SQLITE_OK != sqlite3_open_v2(path, &tmp->db, flags, NULL)) {
            set_generic_error(error, "can't open sqlite database %s: %s", path, sqlite3_errmsg(tmp->db));
            break;
        }
        // preprepare own statement
        if (!sqlite_stmt_prepare(tmp, statements, ARRAY_SIZE(statements), error)) {
            break;
        }
        // fetch user_version
        if (SQLITE_ROW != sqlite3_step(statements[STMT_GET_USER_VERSION].prepared)) {
            set_generic_error(error, "can't retrieve database version: %s", sqlite3_errmsg(tmp->db));
            break;
        }
        tmp->user_version = sqlite3_column_int64(statements[STMT_GET_USER_VERSION].prepared, 0);
        sqlite3_reset(statements[STMT_GET_USER_VERSION].prepared);
        if (env_get_option("SQLITE_TRACE", false)) {
            sqlite3_trace_v2(tmp->db, SQLITE_TRACE_STMT, sqlite_trace_callback, NULL);
        }
        *dbh = tmp;
        status = EPKG_OK;
    } while (false);

    return status;
}

void sqlite_close(sqlite_db_t *dbh)
{
    assert(NULL != dbh);
    assert(NULL != dbh->db);

    sqlite_stmt_finalize(statements, ARRAY_SIZE(statements));
    sqlite3_close(dbh->db);
    free(dbh);
    sqlite3_shutdown();
}

int sqlite_last_insert_id(sqlite_db_t *dbh)
{
    return sqlite3_last_insert_rowid(dbh->db);
}

int sqlite_affected_rows(sqlite_db_t *dbh)
{
    return sqlite3_changes(dbh->db);
}

bool sqlite_create_or_migrate(sqlite_db_t *dbh, const char *table_name, const char *create_stmt, sqlite_migration_t *migrations, size_t migrations_count, char **error)
{
    int ret;
    size_t i;
    sqlite3_stmt *stmt;
    char *errmsg, *buffer;

    if (NULL == (buffer = sqlite3_mprintf("PRAGMA table_info(%w)", table_name))) {
        set_generic_error(error, "can't create table %s: %s", table_name, "buffer overflow");
        return false;
    }
    if (SQLITE_OK == (ret = sqlite3_prepare_v2(dbh->db, buffer, -1, &stmt, NULL))) {
        int step;

        step = sqlite3_step(stmt);
        sqlite3_reset(stmt);
        sqlite3_finalize(stmt);
        switch (step) {
            case SQLITE_DONE:
                ret = sqlite3_exec(dbh->db, create_stmt, NULL, NULL, &errmsg);
                break;
            case SQLITE_ROW:
                for (i = 0; SQLITE_OK == ret && i < migrations_count; i++) {
                    if (migrations[i].version > dbh->user_version) {
                        ret = sqlite3_exec(dbh->db, migrations[i].statement, NULL, NULL, &errmsg);
                    }
                }
                break;
            default:
                // NOP: error is handled below
                break;
        }
    }
    if (SQLITE_OK != ret) {
        set_sqlite_exec_error(error, errmsg, buffer);
        sqlite3_free(errmsg);
    }
    sqlite3_free(buffer);

    return SQLITE_OK == ret;
}

bool sqlite_stmt_prepare(sqlite_db_t *dbh, sqlite_statement_t *statements, size_t count, char **error)
{
    bool ok;
    size_t i;

    ok = true;
    for (i = 0; ok && i < count; i++) {
        ok &= SQLITE_OK == sqlite3_prepare_v2(dbh->db, statements[i].statement, -1, (sqlite3_stmt **) &statements[i].prepared, NULL);
    }
    if (!ok) {
        --i; // return on the statement which actually failed
        set_sqlite_stmt_error(error, dbh->db, &statements[i]);
        while (i-- != 0) { // finalize the initialized ones
            sqlite3_finalize(statements[i].prepared);
            statements[i].prepared = NULL;
        }
    }

    return ok;
}

void sqlite_stmt_finalize(sqlite_statement_t *statements, size_t count)
{
    size_t i;

    for (i = 0; i < count; i++) {
        if (NULL != statements[i].prepared) {
            sqlite3_finalize(statements[i].prepared);
        }
    }
}

static bool sqlite_exec(sqlite_db_t *dbh, const char *query, char **error)
{
    int ret;
    char *errmsg;

    errmsg = NULL;
    if (SQLITE_OK != (ret = sqlite3_exec(dbh->db, query, NULL, NULL, &errmsg))) {
        if (NULL != errmsg) {
            set_sqlite_exec_error(error, errmsg, query);
            sqlite3_free(errmsg);
        }
    }

    return SQLITE_OK == ret;
}

bool sqlite_transaction_begin(sqlite_db_t *dbh, char **error)
{
    return sqlite_exec(dbh, "BEGIN", error);
}

bool sqlite_transaction_commit(sqlite_db_t *dbh, char **error)
{
    return sqlite_exec(dbh, "COMMIT", error);
}

bool sqlite_transaction_rollback(sqlite_db_t *dbh, char **error)
{
    return sqlite_exec(dbh, "ROLLBACK", error);
}
