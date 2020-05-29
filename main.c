#include "coroutine.h"
#include <stdio.h>
#include <assert.h>

struct args {
	int n;
};

static void
foo(schedule_t *s, void *ud) {
	struct args * arg = ud;
	int start = arg->n;
	int i;
	for (i=0;i<5;i++) {
		printf("coroutine %p : %d\n",coroutine_running(s) , start + i);
		coroutine_yield(s);
	}
}

static void
test(schedule_t *s) {
	struct args arg1 = { 0 };
	struct args arg2 = { 100 };

	assert(create_coroutine(s, foo, &arg1) == 0);
	assert(create_coroutine(s, foo, &arg2) == 0);
    
	printf("main start\n");
	while (coroutine_resume(s) == 0) {
	} 
	printf("main end\n");
}

int 
main() {
	schedule_t *s = create_schedule();
	test(s);
	destroy_schedule(s);
	
	return 0;
}

