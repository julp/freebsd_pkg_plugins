#include <stdlib.h> /* malloc(3) */
#include <stdbool.h>
#include <sys/event.h> /* kqueue(2), kevent(2) */
#include <unistd.h> /* fork(2) */
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <time.h>

#include "common.h"
#include "error.h"
#include "process_monitor.h"
#include "shared/argv.h"
#include "kissc/hashtable.h"

struct process_monitor_t {
    int kq;
    HashTable children;
    int active_children_count;
};

typedef struct {
    void *data;
    bool exited;
    int status;
    const char **argv;
} child_t;

static child_t *child_create(const char **argv, void *data, char **error)
{
    child_t *c;

    if (NULL == (c = malloc(sizeof(*c)))) {
        set_malloc_error(error, sizeof(*c));
    } else {
        c->argv = argv;
        c->data = data;
        c->exited = false;
        c->status = EXIT_FAILURE;
    }

    return c;
}

static void child_destroy(child_t *c)
{
    argv_free(c->argv);
    free(c);
}

process_monitor_t *process_monitor_create(char **error)
{
    process_monitor_t *pm;

    if (NULL == (pm = malloc(sizeof(*pm)))) {
        set_malloc_error(error, sizeof(*pm));
    } else {
        pm->active_children_count = 0;
        if (-1 == (pm->kq = kqueue())) {
            set_system_error(error, "kqueue(2) failed");
            free(pm);
            pm = NULL;
        }
        hashtable_init(&pm->children, 32, NULL, NULL, NULL, NULL, (DtorFunc) child_destroy);
    }

    return pm;
}

void process_monitor_clear(process_monitor_t *pm)
{
#if 0
    pid_t pid;
    child_t *c;
    Iterator it;
#endif

    assert(NULL != pm);

    pm->active_children_count = 0;
#if 0
    // NOTE: if I'm not mistaken, it seems that kevent segfaults on a previously killed child
    hashtable_to_iterator(&it, &pm->children);
    for (iterator_first(&it); iterator_is_valid(&it, &pid, &c); iterator_next(&it)) {
        struct kevent ke;

        if (!c->exited) {
            EV_SET(&ke, pid, EVFILT_PROC, EV_DELETE, NOTE_EXIT, 0, NULL);
            kevent(pm->kq, &ke, 1, NULL, 0, NULL);
        }
    }
    iterator_close(&it);
#endif
    hashtable_clear(&pm->children);
}

void process_monitor_destroy(process_monitor_t *pm)
{
    assert(NULL != pm);

    if (-1 != pm->kq) {
        close(pm->kq);
    }
    free(pm);
}

static bool register_pid(process_monitor_t *pm, const char **argv, pid_t pid, void *data, char **error)
{
    bool ok;

    ok = false;
    do {
        child_t *c;
        struct kevent ke;

        EV_SET(&ke, pid, EVFILT_PROC, EV_ADD, NOTE_EXIT, 0, NULL);
        if (-1 == kevent(pm->kq, &ke, 1, NULL, 0, NULL)/* && ESRCH != errno*/) {
            set_system_error(error, "kevent(2) failed");
            break;
        }
        if (NULL == (c = child_create(argv, data, error))) {
            break;
        }
        pm->active_children_count++;
        hashtable_direct_put(&pm->children, HT_PUT_ON_DUP_KEY_PRESERVE, pid, c, NULL);
        ok = true;
    } while (false);

    return ok;
}

static void unregister_pid(process_monitor_t *pm, pid_t pid, exited_fn_t exited, void *exited_acc)
{
    child_t *c;
    int ret, pstat;
    struct kevent ke;

    EV_SET(&ke, pid, EVFILT_PROC, EV_DELETE, NOTE_EXIT, 0, NULL);
    kevent(pm->kq, &ke, 1, NULL, 0, NULL);
    pm->active_children_count--;
    ret = waitpid(pid, &pstat, WNOHANG);
    assert(-1 != ret/* && ECHILD != errno*/);
    (void) ret; // quiet warning variable 'ret' set but not used when assert is turned off
    if (hashtable_direct_get(&pm->children, pid, &c)) {
        c->exited = true;
        c->status = WEXITSTATUS(pstat);
#if 0
        {
            char cmd[ARG_MAX];

            if (argv_join(c->argv, cmd, cmd + STR_SIZE(cmd), NULL)) {
                debug("pid %d (%s) exited with value %d", pid, cmd, c->status);
            }
        }
#endif
        if (NULL != exited) {
            exited(pid, c->status, exited_acc, c->data);
        }
//         hashtable_direct_delete(&pm->children, pid, true);
    }
}

static int (*execcalls[])(const char *, char *const []) = {
    execvp,
    execv,
};

bool process_monitor_exec(process_monitor_t *pm, const char **argv, void *data, pid_t *pid, char **error)
{
    bool ok;

    ok = false;
    do {
        pid_t p;
        const char *path, *slash;

        if (NULL == (argv = (const char **) argv_copy(argv, error))) {
            break;
        }
        path = argv[0];
        if (NULL != (slash = strrchr(argv[0], '/'))) {
            argv[0] = slash + 1;
        }
        p = fork();
        if (-1 == p) {
            argv_free(argv);
            set_system_error(error, "fork(2) failed");
            break;
        } else if (0 == p) {
            execcalls[NULL != slash](path, (char * const *) argv);
            exit(EXIT_FAILURE);
        } else {
            argv[0] = path;
            register_pid(pm, argv, p, data, error);
            if (NULL != pid) {
                *pid = p;
            }
        }
        ok = true;
    } while (false);

    return ok;
}

void process_monitor_await(process_monitor_t *pm, int timeout, exited_fn_t exited, void *exited_acc, hanging_fn_t hanging, void *hanging_acc, char **error)
{
    child_t *c;
    Iterator it;
    ht_key_t pid;
    struct kevent event;
    struct timespec ts, tm, *tmp;

    if (0 == timeout) {
        tmp = NULL;
    } else {
        tmp = &tm;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout;
    }
    while (0 != pm->active_children_count) {
        if (0 != timeout) {
            clock_gettime(CLOCK_REALTIME, &tm);
            tm.tv_sec = ts.tv_sec - tm.tv_sec;
            tm.tv_nsec = ts.tv_nsec - tm.tv_nsec;
            if (tm.tv_nsec < 0) {
                tm.tv_sec--;
                tm.tv_nsec += 1000000000;
            }
            if (tm.tv_sec < 0 || (tm.tv_sec == 0 && tm.tv_nsec == 0)) {
                break;
            }
        }
        switch (kevent(pm->kq, NULL, 0, &event, 1, tmp)) {
            case -1:
                if (EINTR == errno) {
                    debug("EINTR");
                } else {
                    set_system_error(error, "kevent(2) failed");
                    goto end;
                }
                break;
            case 0:
                goto end;
                break;
            case 1:
                if (EVFILT_PROC == event.filter) {
                    assert(NOTE_EXIT == event.fflags);
                    unregister_pid(pm, (pid_t) event.ident, exited, exited_acc);
                }
                break;
            default:
                assert(false);
                break;
        }
    }

end:
    hashtable_to_iterator(&it, &pm->children);
    for (iterator_first(&it); iterator_is_valid(&it, &pid, &c); iterator_next(&it)) {
        if (c->exited) {
            continue;
        }
#if 0
        {
            char cmd[ARG_MAX];

            if (argv_join(c->argv, cmd, cmd + STR_SIZE(cmd), NULL)) {
                debug("killing still running pid %d (%s)", pid, cmd);
            }
        }
#endif
        if (NULL != hanging) {
            hanging(pid, hanging_acc, c->data);
        }
        if (0 != kill(pid, SIGTERM) && ESRCH != errno) {
            char cmd[ARG_MAX];

            if (!argv_join(c->argv, cmd, cmd + STR_SIZE(cmd), error)) {
                break;
            }
            set_system_error(error, "failed to kill(2) PID %d (%s)", pid, cmd);
            break;
        }
    }
    iterator_close(&it);
}
