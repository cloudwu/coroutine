#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#if __APPLE__ && __MACH__
    #include <sys/ucontext.h>
#else 
    #include <ucontext.h>
#endif 

#include "coroutine.h"

#define STACK_SIZE (1024 * 1024)

struct coroutine {
    coroutine_func  func;
    void           *arg;
    ucontext_t      ctx;
    schedule_t     *sched;
    ptrdiff_t       cap;
    ptrdiff_t       size;
    unsigned int    id;
    const char     *sem_down_func;
    int             sem_down_line;
    int             status;
    char           *stack;
    coroutine_t    *next;  // coroutine list entry
};

struct schedule {
    char         stack[STACK_SIZE];
    ucontext_t   main;
    int          id;       // schedule id
    int          co_num;   // total coroutine number
    unsigned int co_id;    // 
    coroutine_t *running;  // now running coroutine, only one
    coroutine_t *wait_co;  // waiting coroutines list for run
};

// one thread can create only one schedule
__thread schedule_t *g_sched_per_thread;
int g_sched_num = 0;

coroutine_t *_new_co(schedule_t *s, coroutine_func func, void *arg) {
    coroutine_t *co = malloc(sizeof(coroutine_t));
    if (co == NULL)
        return NULL;

    memset(co, 0, sizeof(coroutine_t));
    co->func = func;
    co->arg = arg;
    co->sched = s;
    co->status = COROUTINE_READY;
    
    return co;
}

void _delete_co(coroutine_t *co) {
    free(co->stack);
    co->stack = NULL;
    free(co);
}

schedule_t *create_schedule(void) {
    schedule_t *s = malloc(sizeof(schedule_t));
    memset(s, 0, sizeof(schedule_t));
    s->id = __sync_fetch_and_add(&g_sched_num, 1);

    return s;
}

static inline void destroy_coroutines(coroutine_t *co) {
    if (co == NULL)
        return;

    coroutine_t *next;

    do {
        next = co->next;
        _delete_co(co);
        co = next;
    } while (next != NULL);
}

void destroy_schedule(schedule_t *s) {
    destroy_coroutines(s->wait_co);
    s->wait_co = NULL;
    
    destroy_coroutines(s->running);
    s->running = NULL;
    
    free(s);
}

// add co into list head
static inline void insert_head(coroutine_t **list, coroutine_t *co) {
    while (1) {
        co->next = *list;
        if (__sync_bool_compare_and_swap(list, co->next, co))
            break;
    }
}

// get list head co
static inline coroutine_t *get_and_remove_head(coroutine_t **list) {
    coroutine_t *co;
    while (1) {
        co = *list;
        if (co == NULL)
            return NULL;
        if (__sync_bool_compare_and_swap(list, co, co->next))
            break;
    }
    
    co->next = NULL;
    return co;
}

// add co into list tail
static inline void insert_tail(coroutine_t **list, coroutine_t *co) {
    coroutine_t *last_co;
    while (1) {
        last_co = *list;
        if (last_co == NULL) {
            if (__sync_bool_compare_and_swap(list, NULL, co))
                break;
        } else {
            while (last_co->next != NULL)
                last_co = last_co->next;
            if (__sync_bool_compare_and_swap(&(last_co->next), NULL, co))
                break;
        }
    }
}

int create_coroutine(schedule_t *s, coroutine_func func, void *arg) {
    coroutine_t *co = _new_co(s, func , arg);
    if (co == NULL)
        return -1;

    co->id = __sync_fetch_and_add(&s->co_id, 1);
    __sync_fetch_and_add(&s->co_num, 1);

    insert_head(&s->wait_co, co);

    return 0;
}

inline int co_create_coroutine(coroutine_func func, void *arg) {
    schedule_t *s = co_sched_self();
    return create_coroutine(s, func, arg);
}

static void mainfunc(uint32_t low32, uint32_t hi32) {
    uintptr_t ptr = (uintptr_t)low32 | ((uintptr_t)hi32 << 32);
    schedule_t *s = (schedule_t *)ptr;
    coroutine_t *co = s->running;
    
    co->func(co->arg);
    
    s->running = NULL;
    _delete_co(co);
    __sync_fetch_and_sub(&s->co_num, 1);
}

int co_resume(void) {
    schedule_t *s = co_sched_self();

    assert(s->running == NULL);

    // get a coroutine run
    coroutine_t *co = get_and_remove_head(&s->wait_co);
    if (co == NULL)
        return -1;
    
    co->next = NULL;
    s->running = co;
    
    switch(co->status) {
        case COROUTINE_READY:
            getcontext(&co->ctx);
            co->ctx.uc_stack.ss_sp = s->stack;
            co->ctx.uc_stack.ss_size = STACK_SIZE;
            co->ctx.uc_link = &s->main;
            co->status = COROUTINE_RUNNING;
            uintptr_t ptr = (uintptr_t)s;
            makecontext(&co->ctx, (void (*)(void)) mainfunc, 2, (uint32_t)ptr, (uint32_t)(ptr>>32));
            swapcontext(&s->main, &co->ctx);
            break;
        case COROUTINE_SUSPEND:
            memcpy(s->stack + STACK_SIZE - co->size, co->stack, co->size);
            co->status = COROUTINE_RUNNING;
            swapcontext(&s->main, &co->ctx);
            break;
        default:
            assert(0);
    }

    return 0;
}

static void _save_stack(coroutine_t *co, char *top) {
    char dummy = 0;
    assert(top - &dummy <= STACK_SIZE);
    if (co->cap < top - &dummy) {
        free(co->stack);
        co->cap = top-&dummy;
        co->stack = malloc(co->cap);
    }
    co->size = top - &dummy;
    memcpy(co->stack, &dummy, co->size);
}

void co_yield(void) {
    schedule_t *s = co_sched_self();
    coroutine_t *co = s->running;
    assert((char *)&co > s->stack);
    _save_stack(co, s->stack + STACK_SIZE);
    co->status = COROUTINE_SUSPEND;
    s->running = NULL;

    // add current co into wait list tail
    insert_tail(&s->wait_co, co);

    swapcontext(&co->ctx , &s->main);
}

inline coroutine_t *get_running_coroutine(void) {
    return g_sched_per_thread->running;
}

inline unsigned int get_running_co_id(void) {
    return get_running_coroutine()->id;
}

inline int get_sched_num(void) {
    return g_sched_num;
}

inline int get_sched_co_num(schedule_t *sched) {
    return sched->co_num;
}

inline void set_thread_sched(schedule_t *sched) {
    g_sched_per_thread = sched;
}

inline int co_sched_self_id(void) {
    return g_sched_per_thread->id;
}
    
inline schedule_t *co_sched_self(void) {
    return g_sched_per_thread;
}


#include <semaphore.h>
co_sem_t g_co_sem_list;      // for debug
sem_t    g_co_sem_list_lock; // for debug

void init_co_sem_system(void) {
    sem_init(&g_co_sem_list_lock, 0, 1);
    
    memset(&g_co_sem_list, 0, sizeof(g_co_sem_list));
    g_co_sem_list.next = &g_co_sem_list;
    g_co_sem_list.prev = &g_co_sem_list;
}

int co_sem_init(co_sem_t *sem, int cnt) {
    sem->co = NULL;
    sem->cnt = cnt;
    
    sem_wait(&g_co_sem_list_lock);
    sem->next = &g_co_sem_list;
    sem->prev = g_co_sem_list.prev;
    g_co_sem_list.prev->next = sem;
    g_co_sem_list.prev = sem;
    g_co_sem_list.cnt++;
    sem_post(&g_co_sem_list_lock);
    
    return 0;
}

int coroutine_sem_up(co_sem_t *sem) {
    int cnt = __sync_fetch_and_add(&sem->cnt, 1);
    if (cnt >= 0) {
        // get a coroutine run
        coroutine_t *co = get_and_remove_head(&sem->co);
        if (co == NULL)
            return cnt;

        __sync_fetch_and_sub(&sem->cnt, 1);

        // add the coroutine into wait list
        insert_head(&co->sched->wait_co, co);
    }

    return cnt;
}

int coroutine_sem_down(co_sem_t *sem, const char *func, int line) {
    int cnt = __sync_fetch_and_sub(&sem->cnt, 1);
    if (cnt <= 0) {
        __sync_fetch_and_add(&sem->cnt, 1); // recover cnt

        // suspend current coroutine
        schedule_t *s = co_sched_self();
        coroutine_t *co = s->running;
        assert((char *)&co > s->stack);
        _save_stack(co, s->stack + STACK_SIZE);
        co->status = COROUTINE_SUSPEND;
        co->sem_down_func = func;
        co->sem_down_line = line;
        s->running = NULL;

        // add current co into sem list head
        insert_head(&sem->co, co);

        swapcontext(&co->ctx , &s->main);
    }

    return cnt;
}

int co_sem_destroy(co_sem_t *sem) {
    while (sem->co) {
        usleep(1000);
    }

    sem_wait(&g_co_sem_list_lock);
    sem->prev->next = sem->next;
    sem->next->prev = sem->prev;
    g_co_sem_list.cnt--;
    sem_post(&g_co_sem_list_lock);
    
    return 0;
}

int print_all_co_sem(void) {
    int cnt = 0;
    
    printf("print all co sem now\n");
    sem_wait(&g_co_sem_list_lock);
    co_sem_t *sem = g_co_sem_list.next;
    while (sem != &g_co_sem_list) {
        printf("sem: %p\n", sem);
        coroutine_t *co = sem->co;
        while (co != NULL) {
            printf("sem: %p, co: %p, func: %s, line: %d\n", sem, co, co->sem_down_func, co->sem_down_line);
            cnt++;
            co = co->next;
        }
        sem = sem->next;
    }
    sem_post(&g_co_sem_list_lock);

    return cnt;
}
    

