all : test_coroutine test_multi_sched test_sem

test_coroutine: test_coroutine.c coroutine.c
	gcc -g -Wall -o $@ $^

test_multi_sched: test_multi_sched.c coroutine.c multi_sched.c
	gcc -g -Wall -o $@ $^ -lpthread 
	
test_sem: test_sem.c coroutine.c multi_sched.c
	gcc -g -Wall -o $@ $^ -lpthread 

check: 
	./test_coroutine 

clean :
	rm test_coroutine test_multi_sched test_sem
