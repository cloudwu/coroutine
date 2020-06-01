#include "coroutine.h"

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

typedef struct {
    schedule_t *sched;
    pthread_t tid;
    int       id;
    int       cpu_id;
} sched_with_core_t;

#define MAX_SCHED_NUM 32
sched_with_core_t g_sched_with_core[MAX_SCHED_NUM];
unsigned int      g_sched_num = 0;
int               g_running = 0;

void *
sched_func(void *arg) {
    sched_with_core_t *sched_with_core = arg;

    while (g_running) {
        while (resume_coroutine(sched_with_core->sched) == 0) {
        } 

        // no coroutine
        usleep(1000);
    }
 
    return NULL;
}
 
pthread_t 
create_sched_with_core(int cpu_id, void *arg) {
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

 
int 
create_multi_sched(int *cpu_id, int cpu_id_num) {
    int num = 0;

    g_running = 1;
    memset(g_sched_with_core, 0, sizeof(g_sched_with_core));

    int i;
    for (i = 0; i < cpu_id_num && i < MAX_SCHED_NUM; i++) {
        schedule_t *sched = create_schedule();
        assert(sched);

        g_sched_with_core[i].sched = sched;
        g_sched_with_core[i].id = i;
        g_sched_with_core[i].cpu_id = cpu_id[i];
        
        pthread_t tid = create_sched_with_core(cpu_id[i], &g_sched_with_core[i]);
        assert(tid > 0);
        
        g_sched_with_core[i].tid = tid;
        num++;
    }

    g_sched_num = num;

    return num;
}

void 
destroy_multi_sched(void) {
    g_running = 0;
    
    int i;
    for (i = 0; i < MAX_SCHED_NUM; i++) {
        if (g_sched_with_core[i].tid == 0)
            break;

        pthread_join(g_sched_with_core[i].tid, NULL);
        destroy_schedule(g_sched_with_core[i].sched);
        
        g_sched_with_core[i].sched = NULL;
        g_sched_with_core[i].tid = 0;
    }
}

unsigned int
get_sched_num(void) {
    return g_sched_num;
}

schedule_t *
get_sched_by_id(unsigned int id) {
    return g_sched_with_core[id].sched;
}
    

