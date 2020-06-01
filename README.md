It's an asymmetric coroutine library (like lua).

### coroutine.c
 You can use create_schedule to create a schedule first, and then create coroutine in this schedule. 
 You should call resume_coroutine in the thread that you call create_schedule, and you can't call it in a coroutine in the same schedule.
 Coroutines in the same schedule share the stack , so you can create many coroutines without worry about memory.
 But switching context will copy the stack the coroutine used.
 The API is thread safe

### multi_sched.c
 You can create multi schedule on the assigned CPUs
 The API is thread safe

Read source for detail.

origin chinese blog : http://blog.codingnow.com/2012/07/c_coroutine.html

powered by axenhook