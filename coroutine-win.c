#include <memory.h>
#include <stdlib.h>
#include <assert.h>

#ifdef WIN32
#include "coroutine.h"
#include <Windows.h>

#define DEFAULT_COROUTINE 16

struct coroutine;

struct schedule {
    void *fiber_ctx;
    int cap;
    int running;
    struct coroutine **co;
};

struct coroutine {
    coroutine_func func;
    void *ud;
    void *fiber_ctx;
    struct schedule * sch;
    int index;
    int status;
};

struct schedule * coroutine_open(void) {
    struct schedule *sch = (struct schedule *) calloc(1, sizeof(*sch));
    sch->cap = DEFAULT_COROUTINE;
    sch->running = -1;
    sch->co = (struct coroutine**) calloc(sch->cap, sizeof(struct coroutine *));
    sch->fiber_ctx = ConvertThreadToFiber(NULL);
    return sch;
}

void _co_delete(struct coroutine *co) {
    DeleteFiber(co->fiber_ctx);
    free(co);
}

void __stdcall fiber_func(void *p) {
    struct coroutine * co = (struct coroutine *) p;
    struct schedule *sch = (struct schedule *)co->sch;
    int index = co->index;
    assert( (0 <= index) && (index < sch->cap) );
    co->func(sch, co->ud);
    co->status = COROUTINE_DEAD; // Just mark it dead, can't delete it.
    sch->running = -1;
    SwitchToFiber(sch->fiber_ctx);
}

struct coroutine * _co_new(struct schedule *sch, int index, coroutine_func func, void *ud) {
    struct coroutine * co = (struct coroutine *) calloc(1, sizeof(*co));
    co->func = func;
    co->ud = ud;
    co->sch = sch;
    co->index = index;
    co->status = COROUTINE_READY;
    co->fiber_ctx = CreateFiber(0, fiber_func, co);
    return co;
}

void coroutine_close(struct schedule *sch) {
    int i;
    for (i=0; i < sch->cap; i++) {
        struct coroutine * co = sch->co[i];
        if (co) {
            _co_delete(co);
        }
    }
    free(sch->co);
    sch->co = NULL;
    free(sch);
}

int coroutine_new(struct schedule *sch, coroutine_func func, void *ud) {
    int id;
    for (id = 0; id < sch->cap; ++id) {
        struct coroutine *co = sch->co[id];
        if (co == NULL || co->status == COROUTINE_DEAD) {
            if (co && co->status == COROUTINE_DEAD) {
                _co_delete(co);
            }
            sch->co[id] = _co_new(sch, id, func, ud);
            return id;
        }
    }
    sch->co = (struct coroutine **) realloc(sch->co, sch->cap * 2 * sizeof(struct coroutine *));
    assert(sch->co);
    memset(sch->co + sch->cap , 0, sizeof(struct coroutine *) * sch->cap);
    sch->co[id] = _co_new(sch, id, func, ud);
    sch->cap *= 2;
    return id;
}

void coroutine_resume(struct schedule *sch, int id) {
    struct coroutine *co;
    assert(sch->running == -1);
    assert(id >=0 && id < sch->cap);
    if ((co = sch->co[id]) == NULL) {
        return;
    }
    switch(co->status) {
    case COROUTINE_READY:
    case COROUTINE_SUSPEND:
        sch->running = id;
        co->status = COROUTINE_RUNNING;
        SwitchToFiber(co->fiber_ctx);
        break;
    default:
        assert(0);
    }
}

int coroutine_status(struct schedule *sch, int id) {
    assert((id >= 0) && (id < sch->cap));
    if (sch->co[id] == NULL) {
        return COROUTINE_DEAD;
    }
    return sch->co[id]->status;
}

int coroutine_running(struct schedule *sch) {
    return sch->running;
}

void coroutine_yield(struct schedule *sch) {
    int id = sch->running;
    assert(id >= 0);
    sch->co[id]->status = COROUTINE_SUSPEND;
    sch->running = -1;
    SwitchToFiber(sch->fiber_ctx);
}

#endif /* WIN32 */
