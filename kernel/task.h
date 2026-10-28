#ifndef ALPHA_OS_TASK_H
#define ALPHA_OS_TASK_H

#define TASK_MAX 8

typedef void (*task_entry_t)(void);

enum task_state {
    TASK_UNUSED = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_TERMINATED
};

struct task {
    unsigned int saved_esp;
    unsigned int stack_base;
    enum task_state state;
    task_entry_t entry;
};

void task_init(void);
int task_create(task_entry_t entry);
void task_schedule(void);
void task_yield(void);

void task_run_current(void);
__attribute__((noreturn)) void task_exit_current(void);

#endif
