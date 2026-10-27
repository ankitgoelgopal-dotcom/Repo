#ifndef QAR_SCHED_H
#define QAR_SCHED_H
#include <stdint.h>
#include "mm.h"
void sched_init(void);
int  task_spawn_user(const char *name, struct aspace *as, uint64_t entry);
void task_exit(int code) __attribute__((noreturn));
void task_yield(void);
void task_sleep_ticks(uint64_t t);
int  current_pid(void);
struct aspace *current_aspace(void);
void sched_tick(void);
void sched_reap(void);
int  sched_live_user_tasks(void);
#endif
