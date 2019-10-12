#include "my_pthread.h"

/* Scheduler State */
static my_pthread_tcb scheduler[20];
static my_pthread_t currentThread;
static my_pthread_t scheduledThread = 0;
static unsigned int newThreadID = 0;

/* Scheduler Function
 * Pick the next runnable thread and swap contexts to start executing
 */
void schedule(int signum)
{	
	signal(SIGPROF, SIG_IGN);
	
	int prevThread = scheduledThread;
	scheduledThread++;
	
	int loop = 0;
	while(scheduler[scheduledThread].status != RUNNABLE){
		if (scheduledThread == 20) {
			scheduledThread = 0;
			loop++;
		}
		else {
			scheduledThread++;
		}
		//printf("%d\n", scheduledThread);
		
		if (loop > 2) return;
	}
	
	// CREATION OF TIME QUANTUM SIGNAL HANDLING
		struct sigaction sa;
	 	struct itimerval timer;

 		/* Install timer_handler as the signal handler for SIGPROF. */
		memset(&sa, 0, sizeof(sa));
	 	sa.sa_handler = &schedule;
 		sigaction(SIGPROF, &sa, NULL);
 	
	 	/* Configure the timer to expire after TIME_QUANTUM_MS... */
 		timer.it_value.tv_sec = 0;
 		timer.it_value.tv_usec = TIME_QUANTUM_MS;
 		/* ... and every TIME_QUANTUM_MS after that. */
 		timer.it_interval.tv_sec = 0;
 		timer.it_interval.tv_usec = TIME_QUANTUM_MS;
 		setitimer(ITIMER_PROF, &timer, NULL);
	
	ucontext_t currContext;
	getcontext(&currContext);
	/*
	if (prevThread == 0){
		//swapcontext(&currContext, &scheduler[scheduledThread].context);
		setcontext(&scheduler[scheduledThread].context);
	}
	else {
	*/
	//printf("swapping context from thread %d to thread %d\n", prevThread, scheduledThread);
	//printf("context being swapped to: %d\ncontext being swapped from: %d\n", scheduler[prevThread].context, scheduler[scheduledThread].context);
	//sleep(2);
		swapcontext(&scheduler[prevThread].context, &scheduler[scheduledThread].context);
	//}
}

/* Create a new TCB for a new thread execution context and add it to the queue
 * of runnable threads. If this is the first time a thread is created, also
 * create a TCB for the main thread as well as initalize any scheduler state.
 */
void my_pthread_create(my_pthread_t *thread, void*(*function)(void*), void *arg)
{	
	int mainSet = 0;
	//set main context
	if (newThreadID == 0) {
		int i = 0;
		for (i = 0; i < 20; i++){
			scheduler[i].tid = -1;
			scheduler[i].status = UNINITIALIZED;
		}
		
		currentThread = newThreadID;
		scheduler[currentThread].tid = newThreadID;
		newThreadID++;
		scheduler[currentThread].status = RUNNABLE;

		mainSet = 1;
		
		// CREATION OF TIME QUANTUM SIGNAL HANDLING
		struct sigaction sa;
	 	struct itimerval timer;

 		/* Install timer_handler as the signal handler for SIGPROF. */
		memset(&sa, 0, sizeof(sa));
	 	sa.sa_handler = &schedule;
 		sigaction(SIGPROF, &sa, NULL);
 	
	 	/* Configure the timer to expire after TIME_QUANTUM_MS... */
 		timer.it_value.tv_sec = 0;
 		timer.it_value.tv_usec = TIME_QUANTUM_MS;
 		/* ... and every TIME_QUANTUM_MS after that. */
 		timer.it_interval.tv_sec = 0;
 		timer.it_interval.tv_usec = TIME_QUANTUM_MS;
 		setitimer(ITIMER_PROF, &timer, NULL);
	}
	
	currentThread = newThreadID;
	scheduler[currentThread].tid = newThreadID;
	scheduler[newThreadID].status = RUNNABLE;
	//newThread.next = scheduler[newThreadID-1];
	
	*thread = newThreadID;
	
	newThreadID++;

	
	//char context_stack[32768];
	int stacksize = 32768;
	getcontext(&scheduler[currentThread].context);
	scheduler[currentThread].context.uc_link = &scheduler[newThreadID].next->context;
	scheduler[currentThread].context.uc_stack.ss_sp = malloc(stacksize);
	scheduler[currentThread].context.uc_stack.ss_size = stacksize;
	makecontext(&scheduler[currentThread].context, (void*)function, 0);
	
	if (mainSet == 1) {
		//printf("main context set\n");
		getcontext(&scheduler[0].context);
	}
}

/* Give up the CPU and allow the next thread to run.
 */
void my_pthread_yield()
{
	schedule(0);
}

/* The calling thread will not continue until the thread with tid thread
 * has finished executing.
 */
void my_pthread_join(my_pthread_t thread)
{
	scheduler[scheduledThread].status = SLEEP;
	scheduler[thread].waitingThread = scheduledThread;
	
	ucontext_t currContext;
	getcontext(&currContext);
	//printf("check1\n");
	schedule(1);
}


/* Returns the thread id of the currently running thread
 */
my_pthread_t my_pthread_self()
{
	return scheduledThread;
	
	return -1;
}

/* Thread exits, setting the state to finished and allowing the next thread
 * to run.
 */
void my_pthread_exit()
{
	scheduler[scheduledThread].status = FINISHED;
	
	if (scheduler[scheduler[scheduledThread].waitingThread].status == SLEEP)
		scheduler[scheduler[scheduledThread].waitingThread].status = RUNNABLE;
	//printf("check2");
	schedule(1);
}
