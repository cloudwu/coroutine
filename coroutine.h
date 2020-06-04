#ifndef __COROUTINE_H__
#define __COROUTINE_H__

#define COROUTINE_DEAD 0
#define COROUTINE_READY 1
#define COROUTINE_RUNNING 2
#define COROUTINE_SUSPEND 3

struct schedule;
typedef struct schedule schedule_t;

struct coroutine;
typedef struct coroutine coroutine_t;

typedef void *(*coroutine_func)(void *);

schedule_t *create_schedule(void);
void        destroy_schedule(schedule_t *);

int   create_coroutine(schedule_t *, coroutine_func, void *);
int   co_resume(void);
void  co_yield(void);
int   co_create_coroutine(coroutine_func, void *);

void  set_thread_sched(schedule_t *);
int   get_sched_co_num(schedule_t *);
int   get_sched_num(void);
int   co_sched_self_id(void);
schedule_t *co_sched_self(void);
unsigned int get_running_co_id(void);

struct co_sem {
    int            cnt;
    coroutine_t   *co;
    struct co_sem *prev;
    struct co_sem *next;
};

typedef struct co_sem co_sem_t;

int  coroutine_sem_up(co_sem_t *);
int  coroutine_sem_down(co_sem_t *sem, const char *func, int line);
int  print_all_co_sem(void);

void init_co_sem_system(void);

int     co_sem_init(co_sem_t *, int);
#define co_sem_up(sem)   coroutine_sem_up((sem));
#define co_sem_down(sem) coroutine_sem_down((sem), __func__, __LINE__);
int     co_sem_destroy(co_sem_t *);


#endif
