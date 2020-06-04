It's an asymmetric coroutine library (like lua).

注意：不能将协程栈中的资源进行协程间共享，我踩过这个坑。原因是协程栈在调度出去时其栈内容会被拷贝到此协程的私有空间，然后原栈内容会被新协程的栈数据覆盖。如果要跨协程使用共享资源（比如说信号量），需将共享资源定义在堆空间（比如说malloc申请）或者使用全局变量，或者在非协程所在线程中定义（其栈空间不会被切出去）

### coroutine.c
1. You can use create_schedule to create a schedule first, and then create coroutine in this schedule. 
2. You should call resume_coroutine in the thread that you call create_schedule, and you can't call it in a coroutine in the same schedule.
3. Coroutines in the same schedule share the stack , so you can create many coroutines without worry about memory.
4. But switching context will copy the stack the coroutine used.
5. support semaphore in multi coroutines
6. The API is thread safe

### multi_sched.c
1. You can create multi schedule on the assigned CPUs

Read source for detail.

origin chinese blog : http://blog.codingnow.com/2012/07/c_coroutine.html

powered by axenhook