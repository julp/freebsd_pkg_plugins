#pragma once

#include "kissc/iterator.h"

typedef int64_t user_version_t;

typedef struct sqlite_db_t sqlite_db_t;

typedef struct {
    user_version_t version;
    const char *statement;
} sqlite_migration_t;

typedef struct {
    const char *statement;
    const char *input_binds;
    const char *output_binds;
    void *prepared;
} sqlite_statement_t;

#define DECL_STMT(sql, inbinds, outbinds) \
    { sql, inbinds, outbinds, NULL }

void sqlite_close(sqlite_db_t *);
pkg_error_t sqlite_open(const char *, int, sqlite_db_t **, char **);

int sqlite_affected_rows(sqlite_db_t *);
int sqlite_last_insert_id(sqlite_db_t *);

void sqlite_stmt_finalize(sqlite_statement_t *, size_t);
bool sqlite_stmt_prepare(sqlite_db_t *, sqlite_statement_t *, size_t, char **);

bool sqlite_set_user_version(sqlite_db_t *, user_version_t, char **);
bool sqlite_create_or_migrate(sqlite_db_t *, const char *, const char *, sqlite_migration_t *, size_t, char **);

void statement_bind(sqlite_statement_t *, ...);
int statement_fetch(sqlite_db_t *, sqlite_statement_t *, char **, ...);
void statement_to_iterator(Iterator *, sqlite_statement_t *, ...);

bool sqlite_transaction_begin(sqlite_db_t *, char **);
bool sqlite_transaction_commit(sqlite_db_t *, char **);
bool sqlite_transaction_rollback(sqlite_db_t *, char **);
