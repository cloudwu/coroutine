#include <stdio.h>
#include <assert.h>

#include "co_coroutine.h"

struct args {
    int n;
};

static void *foo(void *ud) {
    struct args *arg = ud;
    int start = arg->n;
    int i;
    for (i = 0;i < 5;i++) {
        printf("sched %d co %u: %d\n", co_sched_self_id(), co_self_id(), start + i);
        co_yield();
    }

    return NULL;
}

int main() {
    struct args arg1 = { 100 };
    struct args arg2 = { 200 };

    co_scheduler_t *s = co_create_scheduler();
    co_set_sched(s);

    assert(co_create(foo, &arg1) == 0);
    assert(co_create(foo, &arg2) == 0);
    
    printf("main start\n");
    while (co_resume() == 0) {
    } 
    printf("main end\n");

    co_destroy_scheduler(s);
    
    return 0;
}

