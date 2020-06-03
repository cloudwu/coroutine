#include <stdio.h>
#include <assert.h>

#include "coroutine.h"

struct args {
    int n;
};

static void *foo(void *ud) {
    struct args *arg = ud;
    int start = arg->n;
    int i;
    for (i = 0;i < 5;i++) {
        printf("sched %d co %u: %d\n", co_sched_self_id(), get_running_co_id(), start + i);
        co_yield();
    }

    return NULL;
}

int main() {
    struct args arg1 = { 100 };
    struct args arg2 = { 200 };

    schedule_t *s = create_schedule();
    set_thread_sched(s);

    assert(co_create_coroutine(foo, &arg1) == 0);
    assert(co_create_coroutine(foo, &arg2) == 0);
    
    printf("main start\n");
    while (co_resume() == 0) {
    } 
    printf("main end\n");

    destroy_schedule(s);
    
    return 0;
}

