#ifndef QAR_SCHED_H
#define QAR_SCHED_H
#include <stdint.h>
#include "mm.h"
void sched_init(void);
#define CAP_NET 1u
int  task_spawn_user(const char *name, struct aspace *as, uint64_t entry, uint32_t caps);
uint32_t current_caps(void);
int *current_fds(void);
uint64_t *current_brk(void);          /* user heap break */
#define USER_HEAP_BASE 0x44000000ULL
#define USER_HEAP_MAX  0x4C000000ULL               /* 8 socket slots, -1 = free */
void task_exit(int code) __attribute__((noreturn));
void task_yield(void);
void task_sleep_ticks(uint64_t t);
int  current_pid(void);
struct aspace *current_aspace(void);
void sched_tick(void);
void sched_reap(void);
int  sched_live_user_tasks(void);
#endif
