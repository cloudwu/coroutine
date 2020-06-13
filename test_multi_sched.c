#include <stdio.h>
#include <assert.h>
#include <unistd.h>

#include "co_multi_sched.h"


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
    int sched_cpu[] = {0, 0, 0};
    int num = co_create_multi_sched(sched_cpu, sizeof(sched_cpu)/sizeof(sched_cpu[0]));
    assert(num > 0);
    
    struct args arg0 = { 100 };
    struct args arg1 = { 200 };
    struct args arg2 = { 300 };
    struct args arg3 = { 100 };
    struct args arg4 = { 200 };
    struct args arg5 = { 300 };

    assert(create_coroutine(co_get_sched_by_id(0), foo, &arg0) == 0);
    assert(create_coroutine(co_get_sched_by_id(1), foo, &arg1) == 0);
    assert(create_coroutine(co_get_sched_by_id(2), foo, &arg2) == 0);
    assert(create_coroutine(co_get_sched_by_id(0), foo, &arg3) == 0);
    assert(create_coroutine(co_get_sched_by_id(1), foo, &arg4) == 0);
    assert(create_coroutine(co_get_sched_by_id(2), foo, &arg5) == 0);

    while (!co_is_all_finished()) {
        usleep(1000);
    }
    
    co_destroy_multi_sched();
    
    return 0;
}

