#include "task.h"
#include "memory/memory.h"

#define TASK_STACK_SIZE 4096U

extern void context_switch(unsigned int *old_esp,
                           unsigned int new_esp);
extern void task_bootstrap(void);

static struct task tasks[TASK_MAX];
static int current_task = -1;
static int last_task = -1;
static unsigned int scheduler_esp = 0;
static int scheduler_active = 0;

static int find_ready_task(int after)
{
    for (int step = 1; step <= TASK_MAX; step++) {
        int index = (after + step) % TASK_MAX;

        if (tasks[index].state == TASK_READY)
            return index;
    }

    return -1;
}

static void reap_terminated_tasks(void)
{
    for (int i = 0; i < TASK_MAX; i++) {
        if (tasks[i].state == TASK_TERMINATED &&
            tasks[i].stack_base != 0) {
            memory_page_free(tasks[i].stack_base);
            tasks[i].stack_base = 0;
            tasks[i].saved_esp = 0;
            tasks[i].entry = 0;
            tasks[i].state = TASK_UNUSED;
        }
    }
}

void task_init(void)
{
    for (int i = 0; i < TASK_MAX; i++) {
        tasks[i].saved_esp = 0;
        tasks[i].stack_base = 0;
        tasks[i].state = TASK_UNUSED;
        tasks[i].entry = 0;
    }

    current_task = -1;
    last_task = -1;
    scheduler_esp = 0;
    scheduler_active = 0;
}

int task_create(task_entry_t entry)
{
    if (entry == 0)
        return -1;

    int slot = -1;

    for (int i = 0; i < TASK_MAX; i++) {
        if (tasks[i].state == TASK_UNUSED) {
            slot = i;
            break;
        }
    }

    if (slot < 0)
        return -1;

    unsigned int stack = memory_page_alloc();

    if (stack == 0)
        return -1;

    unsigned int *sp =
        (unsigned int *)(stack + TASK_STACK_SIZE);

    /* context_switch restores EDI, ESI, EBX, EBP, then RET. */
    *--sp = (unsigned int)task_bootstrap;
    *--sp = 0; /* EBP */
    *--sp = 0; /* EBX */
    *--sp = 0; /* ESI */
    *--sp = 0; /* EDI */

    tasks[slot].saved_esp = (unsigned int)sp;
    tasks[slot].stack_base = stack;
    tasks[slot].state = TASK_READY;
    tasks[slot].entry = entry;

    return slot;
}

void task_schedule(void)
{
    if (scheduler_active)
        return;

    scheduler_active = 1;

    for (;;) {
        reap_terminated_tasks();

        int next = find_ready_task(last_task);

        if (next < 0)
            break;

        current_task = next;
        tasks[next].state = TASK_RUNNING;
        last_task = next;

        context_switch(&scheduler_esp, tasks[next].saved_esp);

        /* A yielding or terminated task returns here. */
        current_task = -1;
    }

    reap_terminated_tasks();
    scheduler_active = 0;
}

void task_yield(void)
{
    if (!scheduler_active || current_task < 0)
        return;

    int previous = current_task;

    tasks[previous].state = TASK_READY;
    current_task = -1;

    context_switch(&tasks[previous].saved_esp, scheduler_esp);
}

void task_run_current(void)
{
    if (current_task >= 0 && tasks[current_task].entry != 0)
        tasks[current_task].entry();
}

__attribute__((noreturn))
void task_exit_current(void)
{
    int previous = current_task;

    if (previous >= 0)
        tasks[previous].state = TASK_TERMINATED;

    current_task = -1;

    context_switch(&tasks[previous].saved_esp, scheduler_esp);

    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}
