#include "signo.h"
#include "../../errno.h"
#include "signal.h"

static signal_entry_t signals[MAX_SIGNALS];

void *register_signal(int sig, void *handler, bool ignored) {
    if (sig < 0 || sig >= MAX_SIGNALS)
        return NULL;
    if (sig == SIGKILL || sig == SIGSTOP) {
    	return (void*)-EINVAL;
    }

    signals[sig].handler = handler;
    signals[sig].ignored = ignored;

    return handler;
}

signal_entry_t *lookup_signal(int sig) {
    if (sig < 0 || sig >= MAX_SIGNALS)
        return NULL;

    return &signals[sig];
}


bool issig(task_t *t) {
	while (t->signal != 0) {
		int sig = __builtin_ctzll(t->signal); // count trailing zeros
		
		if (sig == SIGCHLD) {
			if (signals[sig].ignored == 1) {
                t->signal &= ~(1ULL << sig);
				// process wants to ignore SIGCHLD
				// TODO: free process table entries of zombie children
				return false;
			} else {
				// process wants to catch SIGCHLD
				return true;
			}
		}

		if (signals[sig].ignored != 1) {
			return true;
		}

		t->signal &= ~(1ULL << sig); // clear signal bit
	}

	return false;
}

/* process signals [and return their handler if applicable] after recognizing their existence */
void *psig(task_t *t) {
	int sig = __builtin_ctzll(t->signal); // count trailing zeros
	t->signal &= ~(1ULL << sig); // clear signal bit
	
	if (signals[sig].ignored == 1) {
		return NULL;
	} else if (signals[sig].handler != NULL) {
		void* handler = signals[sig].handler;
		signals[sig].handler = NULL; // clear handler
		return handler;
	} else if (sig == SIGABRT || sig == SIGBUS || sig == SIGFPE ||
    sig == SIGILL  || sig == SIGSEGV || sig == SIGTRAP ||
    sig == SIGXCPU || sig == SIGXFSZ) {
    // TODO: dump core in current directory
}
// default behaviour
task_exit();
}

void signal_raise(task_t *t, int sig) {
    t->signal |= (1ULL << sig);
}