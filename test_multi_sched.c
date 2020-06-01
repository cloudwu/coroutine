#include "coroutine.h"
#include "multi_sched.h"

#include <stdio.h>
#include <assert.h>
#include <unistd.h>


struct args {
    int n;
};

static void
foo(schedule_t *s, void *ud) {
    struct args * arg = ud;
    int start = arg->n;
    int i;
    for (i = 0;i < 5;i++) {
        printf("coroutine %p : %d\n", get_running_coroutine(s), start + i);
        yield_coroutine(s);
    }
}


int 
main() {
    int cpu_id[] = {0, 0, 0};
    int num = create_multi_sched(cpu_id, sizeof(cpu_id)/sizeof(cpu_id[0]));
    assert(num > 0);
    
    struct args arg0 = { 100 };
    struct args arg1 = { 200 };
    struct args arg2 = { 300 };
    struct args arg3 = { 100 };
    struct args arg4 = { 200 };
    struct args arg5 = { 300 };

    assert(create_coroutine(get_sched_by_id(0), foo, &arg0) == 0);
    assert(create_coroutine(get_sched_by_id(1), foo, &arg1) == 0);
    assert(create_coroutine(get_sched_by_id(2), foo, &arg2) == 0);
    assert(create_coroutine(get_sched_by_id(0), foo, &arg3) == 0);
    assert(create_coroutine(get_sched_by_id(1), foo, &arg4) == 0);
    assert(create_coroutine(get_sched_by_id(2), foo, &arg5) == 0);

    int t = 5;
    while (t--) {
        printf("loop: %d\n", t);
        sleep(1);
    } 
    
    destroy_multi_sched();
    
    return 0;
}

