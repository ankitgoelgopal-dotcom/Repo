#include "kernel.h"
#include "cpu.h"
#include "sched.h"
#include "mem.h"

enum { T_FREE, T_READY, T_RUNNING, T_SLEEP, T_ZOMBIE };
struct task {
    int pid, state, code; char name[32];
    uint64_t sp, wake, entry; uint8_t *kstack; struct aspace *as;
};
#define MAX_TASKS 32
#define KSTACK 16384
static struct task *tasks[MAX_TASKS];
static struct task *cur;
static int next_pid = 1;

int current_pid(void) { return cur ? cur->pid : 0; }
struct aspace *current_aspace(void) { return cur ? cur->as : 0; }

void sched_init(void) {            /* the boot thread becomes pid 0 (idle) */
    struct task *t = kmalloc(sizeof *t); memset(t, 0, sizeof *t);
    t->pid = 0; t->state = T_RUNNING; memcpy(t->name, "idle", 5);
    tasks[0] = cur = t;
}

static void schedule(void) {
    int start = 0; for (int i = 0; i < MAX_TASKS; i++) if (tasks[i] == cur) start = i;
    struct task *next = 0;
    for (int k = 1; k <= MAX_TASKS && !next; k++) {
        struct task *t = tasks[(start + k) % MAX_TASKS];
        if (!t || t->pid == 0) continue;
        if (t->state == T_SLEEP && ticks >= t->wake) t->state = T_READY;
        if (t->state == T_READY || t->state == T_RUNNING) next = t;
    }
    if (!next) next = tasks[0];                           /* nothing runnable: idle */
    if (next == cur) return;
    struct task *prev = cur;
    if (prev->state == T_RUNNING) prev->state = T_READY;
    next->state = T_RUNNING; cur = next;
    if (next->kstack) tss_set_rsp0((uint64_t)next->kstack + KSTACK);
    aspace_activate(next->as);
    switch_context(&prev->sp, next->sp);
}

void sched_tick(void) { schedule(); }
void task_yield(void) { cli(); schedule(); }
void task_sleep_ticks(uint64_t t) { cli(); cur->wake = ticks + t; cur->state = T_SLEEP; schedule(); }

void task_exit(int code) {
    cli(); cur->code = code; cur->state = T_ZOMBIE;
    kprintf("[proc] pid %d (%s) exited with code %d\n", cur->pid, cur->name, code);
    schedule();
    kpanic("zombie task was rescheduled");
    for (;;) ;
}

static void task_main(void) {          /* first code run by every new task, on its own kernel stack */
    enter_user(cur->entry, USER_TOP - 16);
    for (;;) ;
}

int task_spawn_user(const char *name, struct aspace *as, uint64_t entry) {
    int slot = -1; for (int i = 1; i < MAX_TASKS; i++) if (!tasks[i]) { slot = i; break; }
    if (slot < 0) return -1;
    struct task *t = kmalloc(sizeof *t); uint8_t *ks = kmalloc(KSTACK);
    if (!t || !ks) return -1;
    memset(t, 0, sizeof *t);
    t->pid = next_pid++; t->state = T_READY; t->as = as; t->entry = entry; t->kstack = ks;
    for (int i = 0; i < 31 && name[i]; i++) t->name[i] = name[i];
    uint64_t *sp = (uint64_t *)(((uint64_t)ks + KSTACK) & ~0xFULL);
    *--sp = 0;                          /* alignment pad: rsp = 8 mod 16 at task_main entry */
    *--sp = (uint64_t)task_main;        /* ret target of switch_context */
    for (int i = 0; i < 6; i++) *--sp = 0;   /* rbp rbx r12-r15 */
    t->sp = (uint64_t)sp; tasks[slot] = t;
    return t->pid;
}

void sched_reap(void) {                /* idle thread frees finished tasks */
    for (int i = 1; i < MAX_TASKS; i++) {
        struct task *t = tasks[i];
        if (t && t->state == T_ZOMBIE && t != cur) {
            tasks[i] = 0; aspace_destroy(t->as); kfree(t->kstack); kfree(t);
        }
    }
}
int sched_live_user_tasks(void) {
    int n = 0; for (int i = 1; i < MAX_TASKS; i++) if (tasks[i]) n++;
    return n;
}
