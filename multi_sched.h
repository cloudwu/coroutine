

#ifndef __MULTI_SCHED_H__
#define __MULTI_SCHED_H__

struct schedule;

typedef struct schedule schedule_t;

int  create_multi_sched(int *cpu_id, int cpu_id_num);
void destroy_multi_sched(void);

inline unsigned int get_sched_num(void);
inline schedule_t  *get_sched_by_id(unsigned int id);
inline int sched_self_id(void);

#endif

