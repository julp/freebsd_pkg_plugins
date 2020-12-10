#include <stdlib.h>
#include <fcntl.h> /* O_RDONLY */
#include <unistd.h> /* getpid */
#include <kvm.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <paths.h> /* _PATH_DEVNULL */
#include <pkg.h>

#include "common.h"
#include "error.h"
#include "kissc/ascii.h"

static const char *TRUTHY[] = {
    "1",
    "on",
    "true",
};

bool env_get_option(const char *name, bool value_if_absent)
{
    bool found;
    const char *value;

    if (NULL == (value = getenv(name))) {
        found = value_if_absent;
    } else {
        size_t i;

        found = false;
        for (i = 0; i < ARRAY_SIZE(TRUTHY) && !found; i++) {
            found = 0 == ascii_strcasecmp(value, TRUTHY[i]);
        }
    }

    return found;
}

const char *system_get_env(const char *name, const char *fallback)
{
    const char *value;

    if (NULL == (value = getenv(name))) {
        return fallback;
    } else {
        return value;
    }
}

const char *localbase(void)
{
    return system_get_env("LOCALBASE", "/usr/local");
}

const char *pkg_dbdir(void)
{
    const char *value;
    const pkg_object *object;

    object = pkg_config_get("PKG_DBDIR");
    value = pkg_object_string(object);

    return value;
}

// NOTE: use 0 for buffer_size for unlimited
char **get_pkg_cmd_line(size_t buffer_size, size_t *args_len, char **error)
{
    kvm_t *kd;
    char **args;

    kd = NULL;
    args = NULL;
    do {
        int pcnt;
        struct kinfo_proc *ki;

        if (NULL == (kd = kvm_open(NULL, _PATH_DEVNULL, NULL, O_RDONLY, NULL))) {
            set_generic_error(error, "kvm_open(3) failed");
            break;
        }
        if (NULL == (ki = kvm_getprocs(kd, KERN_PROC_PID, getpid(), &pcnt))) {
            set_generic_error(error, "kvm_getprocs(3) failed: %s", kvm_geterr(kd));
            break;
        }
        if (1 != pcnt) {
            set_generic_error(error, "pkg process not found");
            break;
        }
        if (NULL == (args = kvm_getargv(kd, ki, buffer_size))) {
            set_generic_error(error, "kvm_getargv(3) failed: %s", kvm_geterr(kd));
            break;
        }
        if (NULL != args_len) {
            char **p;

            for (p = args; NULL != *p; p++) {
                // NOP
            }
            *args_len = (size_t) (p - args);
            debug("*args_len = %zu", *args_len);
        }
    } while (false);
    if (NULL != kd) {
        kvm_close(kd);
    }

    return args;
}
