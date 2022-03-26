#include <memory.h>
#include <stdlib.h>
#include <assert.h>

#ifdef WIN32
#include "coroutine.h"
#include <Windows.h>

#define DEFAULT_COROUTINE 16

struct coroutine;

struct schedule {
    void *fiber;
    int cap;
    int running_index;
    struct coroutine **co;
    int unique_id;
};

struct coroutine {
    coroutine_func func;
    void *ud;
    void *fiber;
    struct schedule * sch;
    int index;
    int unique_id;
    int status;
};

int generate_id(struct schedule *sch) {
    return ++sch->unique_id;
}

struct coroutine * get_coroutine_from_id(struct schedule *sch, int id) {
    int index;
    struct coroutine *co;
    for (index = 0; index < sch->cap; ++index) {
        struct coroutine *co = sch->co[index];
        if (co && co->unique_id == id) {
            return co;
        }
    }
    return NULL;
}

struct schedule * coroutine_open(void) {
    struct schedule *sch = (struct schedule *) calloc(1, sizeof(*sch));
    sch->cap = DEFAULT_COROUTINE;
    sch->running_index = -1;
    sch->co = (struct coroutine**) calloc(sch->cap, sizeof(struct coroutine *));
    sch->fiber = ConvertThreadToFiber(NULL);
    return sch;
}

void _co_delete(struct coroutine *co) {
    DeleteFiber(co->fiber);
    free(co);
}

void __stdcall fiber_func(void *p) {
    struct coroutine * co = (struct coroutine *) p;
    struct schedule *sch = (struct schedule *)co->sch;
    int index = co->index;
    assert( (0 <= index) && (index < sch->cap) );
    co->func(sch, co->ud);
    co->status = COROUTINE_DEAD; // Just mark it dead, can't delete it.
    sch->running_index = -1;
    SwitchToFiber(sch->fiber);
}

struct coroutine * _co_new(struct schedule *sch, int index, coroutine_func func, void *ud) {
    struct coroutine * co = (struct coroutine *) calloc(1, sizeof(*co));
    co->func = func;
    co->ud = ud;
    co->sch = sch;
    co->index = index;
    co->unique_id = generate_id(sch);
    co->status = COROUTINE_READY;
    co->fiber = CreateFiber(0, fiber_func, co);
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
    int index;
    struct coroutine *co;
    for (index = 0; index < sch->cap; ++index) {
        struct coroutine *co = sch->co[index];
        if (co == NULL || co->status == COROUTINE_DEAD) {
            if (co && co->status == COROUTINE_DEAD) {
                _co_delete(co);
            }
            co = _co_new(sch, index, func, ud);
            sch->co[index] = co;
            return co->unique_id;
        }
    }
    sch->co = (struct coroutine **) realloc(sch->co, sch->cap * 2 * sizeof(struct coroutine *));
    assert(sch->co);
    memset(sch->co + sch->cap , 0, sizeof(struct coroutine *) * sch->cap);
    co = _co_new(sch, index, func, ud);
    sch->co[index] = co;
    sch->cap *= 2;
    return co->unique_id;
}

void coroutine_resume(struct schedule *sch, int id) {
    struct coroutine *co = get_coroutine_from_id(sch, id);
    int index;
    if (co == NULL) {
        assert(!"something went wrong!");
        return;
    }
    index = co->index;
    assert(sch->running_index == -1);
    assert(0 <= index && index < sch->cap);
    assert(co == sch->co[index]);
    switch(co->status) {
    case COROUTINE_READY:
    case COROUTINE_SUSPEND:
        sch->running_index = index;
        co->status = COROUTINE_RUNNING;
        SwitchToFiber(co->fiber);
        break;
    default:
        assert(0);
    }
}

int coroutine_status(struct schedule *sch, int id) {
    struct coroutine *co = get_coroutine_from_id(sch, id);
    int index;
    if (co == NULL) {
        assert(!"something went wrong!");
        return COROUTINE_DEAD;
    }
    index = co->index;
    assert((0 <= index) && (index < sch->cap));
    return co->status;
}

int coroutine_running(struct schedule *sch) {
    int index = sch->running_index;
    return index >= 0 ? sch->co[index]->unique_id : -1;
}

void coroutine_yield(struct schedule *sch) {
    int index = sch->running_index;
    assert(index >= 0);
    sch->co[index]->status = COROUTINE_SUSPEND;
    sch->running_index = -1;
    SwitchToFiber(sch->fiber);
}

#endif /* WIN32 */
