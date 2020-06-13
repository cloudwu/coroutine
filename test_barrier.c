#include <stdio.h>
#include <assert.h>
#include <unistd.h>

#include "co_multi_sched.h"


struct args {
    int      n;
};

co_barrier_t g_barrier;

static void *foo0(void *ud) {
    struct args *arg = ud;
    
    printf("sched %d co %u: %d\n", co_sched_self_id(), co_self_id(), arg->n++);
    sleep(1);
    co_barrier_wait(&g_barrier);
    //co_barrier_wait(&g_barrier);
    //co_barrier_wait(&g_barrier);
    printf("sched %d co %u: %d\n", co_sched_self_id(), co_self_id(), arg->n++);
    
    
    return NULL;
}

static void *foo1(void *ud) {
    struct args *arg = ud;
    
    printf("sched %d co %u: %d\n", co_sched_self_id(), co_self_id(), arg->n++);
    sleep(2);
    co_barrier_wait(&g_barrier);
    //co_barrier_wait(&g_barrier);
    //co_barrier_wait(&g_barrier);
    printf("sched %d co %u: %d\n", co_sched_self_id(), co_self_id(), arg->n++);
    
    
    return NULL;
}

int main() {
    int sched_cpu[] = {0, 0};
    int num = co_create_multi_sched(sched_cpu, sizeof(sched_cpu)/sizeof(sched_cpu[0]));
    assert(num > 0);
    
    struct args arg0 = { 100 };
    struct args arg1 = { 200 };
    struct args arg2 = { 300 };
    struct args arg3 = { 400 };

    co_barrier_init(&g_barrier, 4);

    printf("test barrier\n");
    assert(create_coroutine(co_get_sched_by_id(0), foo0, &arg0) == 0);
    assert(create_coroutine(co_get_sched_by_id(1), foo1, &arg1) == 0);
    assert(create_coroutine(co_get_sched_by_id(0), foo1, &arg2) == 0);
    assert(create_coroutine(co_get_sched_by_id(1), foo0, &arg3) == 0);

    int t = 5;
    while (t--) {
        printf("loop: %d\n", t);
        sleep(1);
    } 

    co_barrier_destroy(&g_barrier);

    co_destroy_multi_sched();
    
    return 0;
}

