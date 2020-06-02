#include <stdio.h>
#include <assert.h>

#include "coroutine.h"

struct args {
    int n;
};

static void
foo(schedule_t *s, void *ud) {
    struct args * arg = ud;
    int start = arg->n;
    int i;
    for (i = 0;i < 5;i++) {
        printf("sched %d: coroutine %p, %d\n", sched_self_id(), get_running_coroutine(s), start + i);
        yield_coroutine(s);
    }
}

int 
main() {
    struct args arg1 = { 100 };
    struct args arg2 = { 200 };

    schedule_t *s = create_schedule();

    set_thread_sched(s);

    assert(create_coroutine(s, foo, &arg1) == 0);
    assert(create_coroutine(s, foo, &arg2) == 0);
    
    printf("main start\n");
    while (resume_coroutine(s) == 0) {
    } 
    printf("main end\n");

    destroy_schedule(s);
    
    return 0;
}

