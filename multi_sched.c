
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

#include "multi_sched.h"
 
typedef struct {
    schedule_t *sched;
    pthread_t   tid;
    int         cpu_id;
} sched_with_cpu_t;

#define MAX_SCHED_NUM 32
sched_with_cpu_t g_sched_with_cpu[MAX_SCHED_NUM];
int              g_running = 0;

void *sched_func(void *arg) {
    sched_with_cpu_t *sched_with_core = arg;
    set_thread_sched(sched_with_core->sched);

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
 
int create_multi_sched(int *cpu_id, int cpu_id_num) {
    int num = 0;

    g_running = 1;
    memset(g_sched_with_cpu, 0, sizeof(g_sched_with_cpu));

    int i;
    for (i = 0; i < cpu_id_num && i < MAX_SCHED_NUM; i++) {
        schedule_t *sched = create_schedule();
        assert(sched);

        g_sched_with_cpu[i].sched = sched;
        g_sched_with_cpu[i].cpu_id = cpu_id[i];
        
        pthread_t tid = create_sched_with_cpu(cpu_id[i], &g_sched_with_cpu[i]);
        assert(tid > 0);
        
        g_sched_with_cpu[i].tid= tid;
        num++;
    }

    return num;
}

void destroy_multi_sched(void) {
    g_running = 0;
    
    int i;
    for (i = 0; i < MAX_SCHED_NUM; i++) {
        if (g_sched_with_cpu[i].tid == 0)
            break;

        pthread_join(g_sched_with_cpu[i].tid, NULL);
        destroy_schedule(g_sched_with_cpu[i].sched);
        
        g_sched_with_cpu[i].sched = NULL;
        g_sched_with_cpu[i].tid = 0;
    }
}

inline schedule_t *get_sched_by_id(unsigned int id) {
    return g_sched_with_cpu[id].sched;
}

inline int is_all_sched_co_finished(void) {
    int i;
    for (i = 0; i < get_sched_num(); i++) {
        if (get_sched_co_num(get_sched_by_id(i)) != 0)
            return 0;
    }

    return 1;
}
    


