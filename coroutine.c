#include "coroutine.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>

#if __APPLE__ && __MACH__
    #include <sys/ucontext.h>
#else 
    #include <ucontext.h>
#endif 

#define STACK_SIZE (1024*1024)
#define DEFAULT_COROUTINE 16

struct coroutine;
typedef struct coroutine coroutine_t;

struct schedule {
    char stack[STACK_SIZE];
    ucontext_t main;
    int co_num;            // total coroutine number
    coroutine_t *running;  // now running coroutine, only one
    coroutine_t *wait_co;  // waiting coroutines list for run
};

struct coroutine {
    coroutine_func func;
    void *arg;
    ucontext_t ctx;
    schedule_t * sch;
    ptrdiff_t cap;
    ptrdiff_t size;
    int status;
    char *stack;
    coroutine_t *next;  // coroutine list entry
};

coroutine_t * 
_new_co(schedule_t *s , coroutine_func func, void *arg) {
    coroutine_t * co = malloc(sizeof(coroutine_t));
    if (co == NULL)
        return NULL;
        
    co->func = func;
    co->arg = arg;
    co->sch = s;
    co->cap = 0;
    co->size = 0;
    co->status = COROUTINE_READY;
    co->stack = NULL;
    
    return co;
}

void
_delete_co(coroutine_t *co) {
    free(co->stack);
    co->stack = NULL;
    free(co);
}

schedule_t *
create_schedule(void) {
    schedule_t *s = malloc(sizeof(schedule_t));
    memset(s, 0, sizeof(schedule_t));
    return s;
}

static inline void
destroy_coroutines(coroutine_t *co) {
    if (co == NULL)
        return;

    coroutine_t *next;

    do {
        next = co->next;
        _delete_co(co);
        co = next;
    } while (next != NULL);
}

void
destroy_schedule(schedule_t *s) {
    destroy_coroutines(s->wait_co);
    s->wait_co = NULL;
    
    destroy_coroutines(s->running);
    s->running = NULL;
    
    free(s);
}

int 
create_coroutine(schedule_t *s, coroutine_func func, void *arg) {
    coroutine_t *co;

    co = _new_co(s, func , arg);
    if (co == NULL)
        return -1;

    ++s->co_num;

    co->next = s->wait_co;
    s->wait_co = co;

    return 0;
}

static void
mainfunc(uint32_t low32, uint32_t hi32) {
    uintptr_t ptr = (uintptr_t)low32 | ((uintptr_t)hi32 << 32);
    schedule_t *s = (schedule_t *)ptr;
    coroutine_t *co = s->running;
    
    co->func(s, co->arg);
    
    s->running = NULL;
    _delete_co(co);
}

int 
coroutine_resume(schedule_t *s) {
    assert(s->running == NULL);
    coroutine_t *co = s->wait_co;  // get a coroutine run
    if (co == NULL)
        return -1;
    
    s->wait_co = co->next;
    s->running = co;
    co->next = NULL;
    
    int status = co->status;
    switch(status) {
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

static void
_save_stack(coroutine_t *co, char *top) {
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

void
coroutine_yield(schedule_t *s) {
    coroutine_t *co = s->running;
    assert((char *)&co > s->stack);
    _save_stack(co, s->stack + STACK_SIZE);
    co->status = COROUTINE_SUSPEND;
    s->running = NULL;

    // add current co into wait list tail
    coroutine_t *last_co = s->wait_co;
    if (last_co == NULL) {
        s->wait_co = co;
    } else {
        while (last_co->next != NULL)
            last_co = last_co->next;
        last_co->next = co;
    }
        
    swapcontext(&co->ctx , &s->main);
}

void * 
coroutine_running(schedule_t * s) {
    return s->running;
}

