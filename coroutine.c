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

/**
云风实现的coroutine 库
http://blog.codingnow.com/2012/07/c_coroutine.html

这是一个很轻量级,很精巧的协程库,我按自己的大致理解给代码加上相关注释,以防忘记.
xiong chuan liang , 2014-10-13
*/

struct coroutine;

struct schedule {
	char stack[STACK_SIZE]; //运行时,co上下文用的就是这块内存. 厉害
	ucontext_t main;	//上下文信息
	int nco;	//实际的co指针个数
	int cap;	//co指针数组大小
	int running;	// 当前处理的co数组id 或 -1 
	struct coroutine **co; //co指针数组
};

// ptrdiff_t 用于表示指针间的"距离",对于指针加减的结果可用这个类型来表示.
struct coroutine {
	coroutine_func func; //用户自定义的函数 
	void *ud;			//用户自定义的函数,所传进来的参数,在此为main中的statuc arg 
	ucontext_t ctx;		//上下文信息就不用多说了
	struct schedule * sch; //指向co所归属的Sch.
	ptrdiff_t cap;		//容量
	ptrdiff_t size;	//占用的栈大小
	int status;		// COROUTINE_SUSPEND/COROUTINE_READY
	char *stack;	//在yield/resume时发挥保存和恢复作用
};

struct coroutine * 
_co_new(struct schedule *S , coroutine_func func, void *ud) {
	struct coroutine * co = malloc(sizeof(*co));
	co->func = func;
	co->ud = ud;
	co->sch = S;
	co->cap = 0;
	co->size = 0;
	co->status = COROUTINE_READY;
	co->stack = NULL;
	return co;
}

void
_co_delete(struct coroutine *co) {
	free(co->stack);
	free(co);
}

struct schedule * 
coroutine_open(void) {
	struct schedule *S = malloc(sizeof(*S));
	S->nco = 0;
	S->cap = DEFAULT_COROUTINE;
	S->running = -1;
	S->co = malloc(sizeof(struct coroutine *) * S->cap);  //给指针数组分配空间
	memset(S->co, 0, sizeof(struct coroutine *) * S->cap);
	return S;
}

void 
coroutine_close(struct schedule *S) {
	int i;
	for (i=0;i<S->cap;i++) {
		struct coroutine * co = S->co[i];
		if (co) {
			_co_delete(co);
		}
	}
	free(S->co);
	S->co = NULL;
	free(S);
}

int 
coroutine_new(struct schedule *S, coroutine_func func, void *ud) {
	struct coroutine *co = _co_new(S, func , ud);
	
	if (S->nco >= S->cap) { //当实际指针数大于默认的时,再扩容一倍
		int id = S->cap;		
		S->co = realloc(S->co, S->cap * 2 * sizeof(struct coroutine *));
		//注意memset起始位置S->co + S->cap,及大小 
		memset(S->co + S->cap , 0 , sizeof(struct coroutine *) * S->cap);
		
		S->co[S->cap] = co; //存放当前co
		
		//扩容了,大小的保存信息也要跟着变
		S->cap *= 2;
		++S->nco;
		return id;
	} else {
		int i;
		for (i=0;i<S->cap;i++) {
		
			//通过取余得id, 这样当超过S->cap时,又从0开始. 很好用
			int id = (i+S->nco) % S->cap;
			if (S->co[id] == NULL) {
				S->co[id] = co;
				++S->nco;
				return id;
			}
		}
	}
	assert(0);
	return -1;
}

static void
mainfunc(uint32_t low32, uint32_t hi32) {	
	uintptr_t ptr = (uintptr_t)low32 | ((uintptr_t)hi32 << 32);
	struct schedule *S = (struct schedule *)ptr;
	int id = S->running;
	//从指针数组中找到对应id的co
	struct coroutine *C = S->co[id]; 
	//真正的main中用户自定义函数的运行在这.  C->ud是参数,即相当于main中的struct arg
	//在用户的这个函数中,在适当的时机,可调用coroutine_yield,
	//让出去处理别的函数,来达到协程所要的效果
	C->func(S,C->ud);  
	
	//全跑完了,就清掉它
	_co_delete(C);
	S->co[id] = NULL;
	--S->nco;
	S->running = -1;
}

void 
coroutine_resume(struct schedule * S, int id) {
	assert(S->running == -1);
	assert(id >=0 && id < S->cap);
	struct coroutine *C = S->co[id];
	if (C == NULL)
		return;
	int status = C->status;
	switch(status) {
	case COROUTINE_READY:
		getcontext(&C->ctx);
				
		// 使用S中的stack内存.即char stack[STACK_SIZE],来保存协程数据
		C->ctx.uc_stack.ss_sp = S->stack;
		C->ctx.uc_stack.ss_size = STACK_SIZE;		
		C->ctx.uc_link = &S->main;
		S->running = id;
		C->status = COROUTINE_RUNNING;
		
		//注意这里从兼容性角度把S这个指针拆了下,原因是大部份编译器在32/64位下,
		//sizeof(指针)返回的长度是不一样的, 32位下是4个字节,64位下是8个字节
		//所以在makecontext()的这个可变参数中,将其拆分成了两个32位的指针来传.
		//附:在执行地址运算时,多用 uintptr_t 和 uint32_t,更清晰,安全性与兼容性更好
		uintptr_t ptr = (uintptr_t)S;
		makecontext(&C->ctx, (void (*)(void)) mainfunc, 2, (uint32_t)ptr, (uint32_t)(ptr>>32)); 
		swapcontext(&S->main, &C->ctx); //切换到当前co,并执行相关函数
		break;
	case COROUTINE_SUSPEND:
	
		//将当前被调用co的C->stack中,把当时保存的stack信息还原回来		
		//	为什么memcpy要这样写,请想想上面的C->ctx.uc_stack.ss_sp = S->stack; 这句.
		//	还记得 yield()时的_save_stack()函数吗,C->size就在这发挥不小的作用了		
		memcpy(S->stack + STACK_SIZE - C->size, C->stack, C->size);
		S->running = id;
		C->status = COROUTINE_RUNNING;
		swapcontext(&S->main, &C->ctx);
		break;
	default:
		assert(0);
	}
}

static void
_save_stack(struct coroutine *C, char *top) { //这玩意很妙
	char dummy = 0;	
	assert(top - &dummy <= STACK_SIZE);
	
	//因为堆栈是从高地址向低地址扩展的,所以在这,top是栈顶地址, top - &dummy 则能得到栈的实际大小
	// 这里可以去回顾一下堆栈的基础知识. 看看它是怎么压栈的.
	if (C->cap < top - &dummy) {
		free(C->stack);	//如果C->stack为空时,执行free,并不是啥大事,不会报错的.
		C->cap = top-&dummy;	//还记得它是什么类型吗?  ptrdiff_t
		C->stack = malloc(C->cap);  //分配内存,用于保存切换时的堆栈信息,然后在resume时从这弄回去
	}
	C->size = top - &dummy; //当前co堆栈所占size
	
	//把栈信息保存起来
	memcpy(C->stack, &dummy, C->size); 
}

void
coroutine_yield(struct schedule * S) {
	int id = S->running;
	assert(id >= 0);
	struct coroutine * C = S->co[id];
	assert((char *)&C > S->stack);
	
	//通过栈copy,把当前上下文保存起来,以便下次使用时还原回来
	_save_stack(C,S->stack + STACK_SIZE); 
	C->status = COROUTINE_SUSPEND;  //挂起来
	S->running = -1;
	swapcontext(&C->ctx , &S->main);
}

int 
coroutine_status(struct schedule * S, int id) {
	assert(id>=0 && id < S->cap);
	if (S->co[id] == NULL) {
		return COROUTINE_DEAD;
	}
	return S->co[id]->status;
}

int 
coroutine_running(struct schedule * S) {
	return S->running;
}

