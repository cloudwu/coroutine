

#ifndef __MULTI_SCHED_H__
#define __MULTI_SCHED_H__

#include "coroutine.h"

int  create_multi_sched(int *cpu_id, int cpu_id_num);
void destroy_multi_sched(void);

schedule_t  *get_sched_by_id(unsigned int id);
int is_all_sched_co_finished(void);

#endif

