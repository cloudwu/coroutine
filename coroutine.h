#ifndef C_COROUTINE_H
#define C_COROUTINE_H

#define COROUTINE_DEAD 0
#define COROUTINE_READY 1
#define COROUTINE_RUNNING 2
#define COROUTINE_SUSPEND 3

struct schedule;
typedef struct schedule schedule_t;


typedef void (*coroutine_func)(schedule_t *, void *arg);

schedule_t *create_schedule(void);
void  destroy_schedule(schedule_t *);

int   create_coroutine(schedule_t *, coroutine_func, void *arg);
int   resume_coroutine(schedule_t *);
void  yield_coroutine(schedule_t *);

inline void *get_running_coroutine(schedule_t *);
inline int   get_sched_num(void);
inline void  set_thread_sched(schedule_t *sched);
inline int   sched_self_id(void);
inline schedule_t *sched_self(void);

struct co_sem;
typedef struct co_sem co_sem_t;

void co_sem_init(co_sem_t *sem);
void co_sem_up(co_sem_t *sem);
void co_sem_down(co_sem_t *sem);
void co_sem_destroy(co_sem_t *sem);

#endif
