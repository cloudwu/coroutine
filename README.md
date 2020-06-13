It's an asymmetric coroutine library (like lua).

### co_coroutine.c: pthread like API
1. coroutine
2. semaphore for coroutine
3. barrier for coroutine

### co_multi_sched.c: a simple scheduler manager
1. multi schedulers on the assigned CPUs

### test_xxx.c
1. test program
2. the API use example

Read source for detail.

origin chinese blog : http://blog.codingnow.com/2012/07/c_coroutine.html

注意：不能将协程栈中的资源进行协程间共享，我踩过这个坑。原因是协程栈在调度出去时其栈内容会被拷贝到此协程的私有空间，然后原栈内容会被新协程的栈数据覆盖。如果要跨协程使用共享资源（比如说信号量），需将共享资源定义在堆空间（比如说malloc申请）或者使用全局变量，或者在非协程所在线程中定义（其栈空间不会被切出去）

powered by axenhook