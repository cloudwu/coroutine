#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <ucontext.h>

#include "co_coroutine.h"

#define STACK_SIZE (1024 * 1024)

typedef enum {
	CO_DEAD,
	CO_READY,
	CO_RUNNING,
	CO_SUSPEND
} co_status_t;

struct co_coroutine {
    co_func_t       func;
    void           *arg;
	
    ucontext_t      ctx;
    ptrdiff_t       cap;
    ptrdiff_t       size;
    co_status_t     status;
    char           *stack;
	
    const char     *suspend_func;  // coroutine suspend point
    int             suspend_line;  // coroutine suspend point
	
    int             id;    // coroutine id
    co_scheduler_t *sched;
    co_coroutine_t *next;  // coroutine list entry
};

struct co_scheduler {
    char            stack[STACK_SIZE];
    ucontext_t      main;
    int             id;       // schedule id
    int             co_num;   // total coroutine number
    unsigned int    co_seq;   // to generate coroutine id
    co_coroutine_t *running;  // now running coroutine, only one
    co_coroutine_t *wait_co;  // waiting coroutines list for run
};

// one thread can create one scheduler only
__thread co_scheduler_t *g_sched_per_thread;
int g_sched_num = 0;

static inline co_coroutine_t *_new_co(co_scheduler_t *sched, co_func_t func, void *arg) {
    co_coroutine_t *co = malloc(sizeof(co_coroutine_t));
    if (co == NULL)
        return NULL;

    memset(co, 0, sizeof(co_coroutine_t));
    co->func = func;
    co->arg = arg;
    co->sched = sched;
    co->status = CO_READY;
    
    return co;
}

static inline void _delete_co(co_coroutine_t *co) {
    free(co->stack);
    co->stack = NULL;
    free(co);
}

static inline void _delete_all_co(co_coroutine_t *co) {
    if (co == NULL)
        return;

    co_coroutine_t *next;

    do {
        next = co->next;
        _delete_co(co);
        co = next;
    } while (next != NULL);
}

co_scheduler_t *co_create_scheduler(void) {
    co_scheduler_t *sched = malloc(sizeof(co_scheduler_t));
    if (sched == NULL)
        return NULL;
	
    memset(sched, 0, sizeof(co_scheduler_t));
    sched->id = __sync_fetch_and_add(&g_sched_num, 1);

    return sched;
}

void co_destroy_scheduler(co_scheduler_t *sched) {
    _delete_all_co(sched->wait_co);
    sched->wait_co = NULL;
    
    _delete_all_co(sched->running);
    sched->running = NULL;
    
    free(sched);
}

inline int co_get_sched_num(void) {
    return g_sched_num;
}

inline int co_get_co_num(co_scheduler_t *sched) {
    return sched->co_num;
}

inline void co_set_sched(co_scheduler_t *sched) {
    g_sched_per_thread = sched;
}

inline co_scheduler_t *co_sched_self(void) {
    return g_sched_per_thread;
}

inline int co_sched_self_id(void) {
    return g_sched_per_thread->id;
}
    

// add co into list head
static inline void _insert_head(co_coroutine_t **list, co_coroutine_t *co) {
    while (1) {
        co->next = *list;
        if (__sync_bool_compare_and_swap(list, co->next, co))
            break;
    }
}

// get list head co
static inline co_coroutine_t *_pop_head(co_coroutine_t **list) {
    co_coroutine_t *co;
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
static inline void _insert_tail(co_coroutine_t **list, co_coroutine_t *co) {
    co_coroutine_t *last_co;
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

int create_coroutine(co_scheduler_t *sched, co_func_t func, void *arg) {
    co_coroutine_t *co = _new_co(sched, func , arg);
    if (co == NULL)
        return -1;

    co->id = __sync_fetch_and_add(&sched->co_seq, 1);
    __sync_fetch_and_add(&sched->co_num, 1);

    _insert_head(&sched->wait_co, co);

    return 0;
}

static void mainfunc(uint32_t low32, uint32_t hi32) {
    uintptr_t ptr = (uintptr_t)low32 | ((uintptr_t)hi32 << 32);
    co_scheduler_t *sched = (co_scheduler_t *)ptr;
    co_coroutine_t *co = sched->running;
    
    co->func(co->arg);
    
    sched->running = NULL;
    _delete_co(co);
    __sync_fetch_and_sub(&sched->co_num, 1);
}

int resume_coroutine(void) {
    co_scheduler_t *sched = co_sched_self();
    assert(sched->running == NULL);

    // get a coroutine run
    co_coroutine_t *co = _pop_head(&sched->wait_co);
    if (co == NULL)
        return -1;
    
    sched->running = co;
    
    switch(co->status) {
        case CO_READY:
            getcontext(&co->ctx);
            co->ctx.uc_stack.ss_sp = sched->stack;
            co->ctx.uc_stack.ss_size = STACK_SIZE;
            co->ctx.uc_link = &sched->main;
            co->status = CO_RUNNING;
            uintptr_t ptr = (uintptr_t)sched;
            makecontext(&co->ctx, (void (*)(void)) mainfunc, 2, (uint32_t)ptr, (uint32_t)(ptr>>32));
            swapcontext(&sched->main, &co->ctx);
            break;
        case CO_SUSPEND:
            memcpy(sched->stack + STACK_SIZE - co->size, co->stack, co->size);
            co->status = CO_RUNNING;
            swapcontext(&sched->main, &co->ctx);
            break;
        default:
            assert(0);
    }

    return 0;
}

static inline void _save_stack(co_coroutine_t *co, char *top) {
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

static inline co_coroutine_t *_prepare_suspend(co_scheduler_t *sched, const char *func, int line) {
    co_coroutine_t *co = sched->running;
    assert((char *)&co > sched->stack);
    _save_stack(co, sched->stack + STACK_SIZE);
    co->status = CO_SUSPEND;
    co->suspend_func = func;
    co->suspend_line = line;
    sched->running = NULL;

	return co;
}

void yield_coroutine(const char *func, int line) {
    co_scheduler_t *sched = co_sched_self();
    co_coroutine_t *co = _prepare_suspend(sched, func, line);
    _insert_tail(&sched->wait_co, co);
    
    swapcontext(&co->ctx , &sched->main);
}

inline co_coroutine_t *co_self(void) {
    return co_sched_self()->running;
}

inline int co_self_id(void) {
    return co_self()->id;
}

#if CO_DESC("semaphore for coroutine")

int co_sem_init(co_sem_t *sem, int cnt) {
    sem->co = NULL;
    sem->cnt = cnt;
    
    return 0;
}

int coroutine_sem_up(co_sem_t *sem) {
    int cnt = __sync_fetch_and_add(&sem->cnt, 1);
    if (cnt >= 0) {
        // get a coroutine run
        co_coroutine_t *co = _pop_head(&sem->co);
        if (co == NULL)
            return cnt;

        __sync_fetch_and_sub(&sem->cnt, 1);

        // add the coroutine into wait list
        _insert_head(&co->sched->wait_co, co);
    }

    return cnt;
}

int coroutine_sem_down(co_sem_t *sem, const char *func, int line) {
    int cnt = __sync_fetch_and_sub(&sem->cnt, 1);
    if (cnt <= 0) {
        __sync_fetch_and_add(&sem->cnt, 1); // recover cnt

        // suspend current coroutine
        co_scheduler_t *sched = co_sched_self();
        co_coroutine_t *co = _prepare_suspend(sched, func, line);
        _insert_head(&sem->co, co);

        swapcontext(&co->ctx , &sched->main);
    }

    return cnt;
}

int co_sem_destroy(co_sem_t *sem) {
    while (sem->co) {
        usleep(1000);
    }

    return 0;
}

#endif

#if CO_DESC("barrier for coroutine")

int co_barrier_init(co_barrier_t *barrier, unsigned num) {
    barrier->cnt = 0;
    barrier->num = num;
    barrier->co = NULL;
    co_spin_lock_init(&barrier->lock);
    return 0;
}

int coroutine_barrier_wait(co_barrier_t *barrier, const char *func, int line) {
    co_spin_lock(&barrier->lock);
    if (++barrier->cnt == barrier->num) { // resume all coroutine
        co_coroutine_t *co;
        int cnt = 0;
        while (1) {
            co = _pop_head(&barrier->co);
            if (co == NULL)
                break;

            // add the coroutine into wait list
            _insert_head(&co->sched->wait_co, co);
            cnt++;
        }

        assert(cnt == barrier->cnt - 1);
        barrier->cnt = 0; // for re-use
        co_spin_unlock(&barrier->lock);
        
        return 0;
    }
    
    // suspend current coroutine
    co_scheduler_t *sched = co_sched_self();
    co_coroutine_t *co = _prepare_suspend(sched, func, line);
    _insert_head(&barrier->co, co);
    co_spin_unlock(&barrier->lock);
    swapcontext(&co->ctx , &sched->main);
    
    return 0;
}

int co_barrier_destroy(co_barrier_t *barrier) {
    return 0;
}

#endif
    

