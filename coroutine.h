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
void destroy_schedule(schedule_t *);

int create_coroutine(schedule_t *, coroutine_func, void *arg);
int coroutine_resume(schedule_t *);
int coroutine_status(schedule_t *, int id);
void *coroutine_running(schedule_t *);
void coroutine_yield(schedule_t *);

#endif
