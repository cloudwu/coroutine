

#ifndef __CO_MULTI_SCHED_H__
#define __CO_MULTI_SCHED_H__

#include "co_coroutine.h"

int  co_create_multi_sched(int *sched_cpu, int sched_num);
void co_destroy_multi_sched(void);

co_scheduler_t  *co_get_sched_by_id(unsigned int id);
int is_all_co_finished(void);

#endif

