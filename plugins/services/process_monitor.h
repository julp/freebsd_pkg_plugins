#pragma once

typedef struct process_monitor_t process_monitor_t;

typedef void (*hanging_fn_t)(pid_t, void *, void *);
typedef void (*exited_fn_t)(pid_t, int, void *, void *);

void process_monitor_clear(process_monitor_t *);
void process_monitor_destroy(process_monitor_t *);
process_monitor_t *process_monitor_create(char **);
bool process_monitor_exec(process_monitor_t *, const char **, void *, pid_t *, char **);
void process_monitor_await(process_monitor_t *, int, exited_fn_t, void *, hanging_fn_t, void *, char **);
