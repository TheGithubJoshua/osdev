#pragma once
#include <stddef.h>
#include <stdbool.h>
#include "../../thread/thread.h"

#define MAX_SIGNALS 64
#define SIG_DFL 0
#define SIG_IGN 1 

typedef struct {
    void *handler;
    bool ignored;
} signal_entry_t;

void *register_signal(int sig, void* func, bool ignored);
bool issig(task_t *t);
void *psig(task_t *t);
void signal_raise(task_t *t, int sig);
signal_entry_t *lookup_signal(int sig);