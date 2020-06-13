#include <stdio.h>
#include <assert.h>
#include <unistd.h>

#include "co_multi_sched.h"


struct args {
    int      n;
    co_sem_t sem0;
    co_sem_t sem1;
};

static void *foo0(void *ud) {
    struct args *arg = ud;
    int start = arg->n;
    int cnt;
    int i;
    for (i = 0;i < 10;i++) {
        cnt = co_sem_up(&arg->sem0);
        printf("foo0 sched %d co %u: sem0 up   %d %d\n", co_sched_self_id(), co_self_id(), start + i, cnt);
        cnt = co_sem_down(&arg->sem1);
        printf("foo0 sched %d co %u: sem1 down %d %d\n", co_sched_self_id(), co_self_id(), start + i, cnt);
    }
    
    return NULL;
}

static void *foo1(void *ud) {
    struct args *arg = ud;
    int start = arg->n;
    int cnt;
    int i;
    for (i = 0;i < 10;i++) {
        cnt = co_sem_down(&arg->sem0);
        printf("foo1 sched %d co %u: sem0 down %d %d\n", co_sched_self_id(), co_self_id(), start + i, cnt);
        cnt = co_sem_up(&arg->sem1);
        printf("foo1 sched %d co %u: sem1 up   %d %d\n", co_sched_self_id(), co_self_id(), start + i, cnt);
    }
    
    return NULL;
}

int main() {
    int sched_cpu[] = {0, 0};
    int num = co_create_multi_sched(sched_cpu, sizeof(sched_cpu)/sizeof(sched_cpu[0]));
    assert(num > 0);
    
    struct args arg0 = { 100 };
    struct args arg1 = { 200 };
    struct args arg2 = { 300 };

    co_sem_init(&arg0.sem0, 0);
    co_sem_init(&arg0.sem1, 0);
    co_sem_init(&arg1.sem0, 0);
    co_sem_init(&arg1.sem1, 0);
    co_sem_init(&arg2.sem0, 0);
    co_sem_init(&arg2.sem1, 0);
    
    // the same sem: on different coroutines, but on the same sched 0
    printf("test 1\n");
    assert(create_coroutine(co_get_sched_by_id(0), foo0, &arg0) == 0);
    assert(create_coroutine(co_get_sched_by_id(0), foo1, &arg0) == 0);
    
    // the same sem: on different coroutines, but on the same sched 1
    printf("test 2\n");
    assert(create_coroutine(co_get_sched_by_id(1), foo0, &arg1) == 0);
    assert(create_coroutine(co_get_sched_by_id(1), foo1, &arg1) == 0);
    
    // the same sem: on different coroutines, and on the different sched
    printf("test 3\n");
    assert(create_coroutine(co_get_sched_by_id(0), foo0, &arg2) == 0);
    assert(create_coroutine(co_get_sched_by_id(1), foo1, &arg2) == 0);
    
    while (!co_is_all_finished()) {
        usleep(1000);
    }
    
    co_sem_destroy(&arg0.sem0);
    co_sem_destroy(&arg0.sem1);
    co_sem_destroy(&arg1.sem0);
    co_sem_destroy(&arg1.sem1);
    co_sem_destroy(&arg2.sem0);
    co_sem_destroy(&arg2.sem1);
    
    co_destroy_multi_sched();
    
    return 0;
}

