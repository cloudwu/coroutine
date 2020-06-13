
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>

#include "co_multi_sched.h"
 
typedef struct {
    co_scheduler_t *sched;
    pthread_t       tid;
    int             cpu_id;
} sched_with_cpu_t;

#define MAX_SCHED_NUM 32
sched_with_cpu_t g_sched_with_cpu[MAX_SCHED_NUM];
int              g_running = 0;

void *sched_func(void *arg) {
    sched_with_cpu_t *sched_with_core = arg;
    co_set_sched(sched_with_core->sched);

    while (g_running) {
        while (co_resume() == 0) {
        } 

        // no coroutine
        usleep(1000);
    }
 
    return NULL;
}

pthread_t create_sched_with_cpu(int cpu_id, void *arg) {
    pthread_t tid;
    pthread_attr_t attr;
 
    pthread_attr_init(&attr);
 
    cpu_set_t cpu_info;
    CPU_ZERO(&cpu_info);
    CPU_SET(cpu_id, &cpu_info);
    if (0 != pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpu_info))
    {
        return -1;
    }
 
    if (0 != pthread_create(&tid, &attr, sched_func, arg))
    {
        return -2;
    }

    return tid;
}
 
int co_create_multi_sched(int *sched_cpu, int sched_num) {
    int num = 0;

    g_running = 1;
    memset(g_sched_with_cpu, 0, sizeof(g_sched_with_cpu));

    int i;
    for (i = 0; i < sched_num && i < MAX_SCHED_NUM; i++) {
        co_scheduler_t *sched = co_create_scheduler();
        assert(sched);

        g_sched_with_cpu[i].sched = sched;
        g_sched_with_cpu[i].cpu_id = sched_cpu[i];
        
        pthread_t tid = create_sched_with_cpu(sched_cpu[i], &g_sched_with_cpu[i]);
        assert(tid > 0);
        
        g_sched_with_cpu[i].tid= tid;
        num++;
    }

    return num;
}

void co_destroy_multi_sched(void) {
    g_running = 0;
    
    int i;
    for (i = 0; i < MAX_SCHED_NUM; i++) {
        if (g_sched_with_cpu[i].tid == 0)
            break;

        pthread_join(g_sched_with_cpu[i].tid, NULL);
        co_destroy_scheduler(g_sched_with_cpu[i].sched);
        
        g_sched_with_cpu[i].sched = NULL;
        g_sched_with_cpu[i].tid = 0;
    }
}

inline co_scheduler_t *co_get_sched_by_id(unsigned int id) {
    return g_sched_with_cpu[id].sched;
}

inline int is_all_co_finished(void) {
    int i;
    for (i = 0; i < co_get_sched_num(); i++) {
        if (co_get_co_num(co_get_sched_by_id(i)) != 0)
            return 0;
    }

    return 1;
}
    


