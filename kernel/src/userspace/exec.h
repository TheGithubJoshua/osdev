#pragma once
#include <stdint.h>
uint64_t load_program(int filedesc);
uintptr_t setup_stack(void *stack_top, char **argvp, char** envp);