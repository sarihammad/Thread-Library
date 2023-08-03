#include "thread.h"  
  
#include <ucontext.h>  
#include <stdlib.h>  
#include <assert.h>  
#include <sys/time.h>  
  

#define DEBUG_USE_VALGRIND
#ifdef DEBUG_USE_VALGRIND  
#include <valgrind/valgrind.h>  
#endif  
  
#include "interrupts.h"  

/**         
 * The Thread States      
 */      
    
typedef enum {          
  READY = 1,          
  RUNNING = 2,          
  EXITED = 3,          
  EMPTY = 4,          
  KILLED = 5,
  BLOCKED = 6  
} State;          
        
/**       
 * The Thread Control Block.       
 */          
       
typedef struct          
{          
  Tid thread_id;          
  ucontext_t context;          
  State state;          
  void *sp;    
  ExitCode exit_code;   
} TCB;          
        
/**      
 * The Node in a Queue.      
 */          
          
typedef struct node {          
  TCB *thread;          
  struct node *next;          
} node;       
  
/** 
 * A wait queue. 
 */  
typedef struct wait_queue_t  
{  
  node* head;  
} WaitQueue;  
  
  
// Current Running Thread          
TCB *running_thread;          
          
// Static Global Array of Threads          
TCB threads[MAX_THREADS];          
          
// Ready Queue of Threads          
WaitQueue rq;  
  
// Static Global Array of Waiting Queues          
WaitQueue wait_queues[MAX_THREADS];      

/**    
 * Stub function to be used for the newly created thread.     
 * The function is called before the thread exits.    
 *     
 * @param f The function to be called.    
 * @param arg the argument to be passed into the function.    
 */           
void thread_stub(void (*f)(void *), void *arg)        
{     
    InterruptsEnable();  
    f(arg);  
    ThreadExit(running_thread->exit_code);     
}  
    
/**  
 * Add the thread with argument thread to the tail of the   
 * ready_queue queue.  
 *   
 * @param queue the pointer to the ready_queue struct of threads  
 * @param thread the thread we intend to add to queue  
 */           
void insert_into_queue(WaitQueue *queue, TCB* thread) {  
  InterruptsState enabled = InterruptsDisable();   
  assert(queue != NULL);  
  assert(thread != NULL);  
  struct node* thread_node = (struct node*) malloc(sizeof(struct node));        
  thread_node->thread = thread;        
  thread_node->next = NULL;  
  
  struct node* curr = queue->head;        
  if (curr == NULL){        
    queue->head = thread_node;  
    InterruptsSet(enabled);        
    return;        
  }        
  while (curr->next != NULL){        
    curr = curr->next;        
  }        
  curr->next = thread_node;  
  InterruptsSet(enabled);        
}    
    
/**  
 * Dequeue the thread at the head of the ready_queue queue and   
 * free the dynamically allocated memory of its node.  
 *   
 * @param queue the pointer to the ready_queue struct of threads  
 *   
 * @return The pointer to the thread at the head of queue.  
 */      
TCB *extract_from_queue(WaitQueue *queue) {   
  InterruptsState enabled = InterruptsDisable();       
  TCB *thread = queue->head->thread;        
  node *temp = queue->head;        
  queue->head = queue->head->next;        
  free(temp);  
  InterruptsSet(enabled);        
  return thread;        
}      
      
/**  
 * Remove the thread with thread ID tid from the ready_queue queue.  
 *   
 * @param queue the pointer to the ready_queue struct of threads  
 * @param tid the thread id of the thread we intend to remove  
 */        
void remove_from_queue(WaitQueue *queue, Tid tid) { 
  InterruptsState enabled = InterruptsDisable();
  assert(queue != NULL);      
  node *curr_node = queue->head;      
      
  if (curr_node != NULL) {      
    if (curr_node->thread->thread_id == tid) {      
      node *temp = curr_node;      
      queue->head = queue->head->next;      
      free(temp);  
      InterruptsSet(enabled);      
      return;      
    }      
    while (curr_node->next != NULL && curr_node->next->thread->thread_id != tid) {  
      curr_node = curr_node->next;      
    }      
    if (curr_node->next != NULL) {      
      node *temp = curr_node->next;      
      curr_node->next = curr_node->next->next;      
      free(temp);  
      InterruptsSet(enabled);      
      return;      
    }      
  }       
}       
  
/**  
 * Remove the thread with thread ID tid from all wait queues.  
 *   
 * @param tid the thread id of the thread we intend to remove  
 */    
void remove_from_all_wait_queues(Tid tid) {      
  InterruptsState enabled = InterruptsDisable();    
  for (int i = 0; i < CSC369_MAX_THREADS; i++) {        
    if (wait_queues[i].head != NULL) {     
      remove_from_queue(&wait_queues[i], tid);  
    }        
  }  
  InterruptsSet(enabled);     
}      
    
/**  
 * Free all dynamically allocated stacks of all exited or killed threads.  
 */        
void free_exited_threads() {    
  InterruptsState enabled = InterruptsDisable();      
  for (int i = 0; i < CSC369_MAX_THREADS; i++) {        
    if (threads[i].state == EXITED || threads[i].state == KILLED) {     
      if (running_thread != &threads[i]) {    
        threads[i].state = EMPTY;  
        free(threads[i].sp);       
      }    
    }        
  }  
  InterruptsSet(enabled);  
}

void print_queue(WaitQueue *queue) {
  InterruptsState enabled = InterruptsDisable(); 
  struct node* curr = queue->head;        
  if (curr == NULL) {
    printf("---EMPTY QUEUE---\n");
    InterruptsSet(enabled);        
    return;        
  }        
  while (curr != NULL){
    printf("%d ->", curr->thread->thread_id);
    curr = curr->next;        
  }
  printf("\n");
  InterruptsSet(enabled);        
  return;  
}
  
  
int          
ThreadInit(void)          
{  
  InterruptsState enabled = InterruptsDisable();  
  threads[0].thread_id = 0;          
  threads[0].state = RUNNING;          
  threads[0].sp = NULL;  
  threads[0].exit_code = 0;  
  wait_queues[0].head = NULL;  
    
  rq.head = NULL;         
    
  for (int i = 1; i < CSC369_MAX_THREADS; i++){          
      threads[i].state = EMPTY;          
  }          
    
  int err = getcontext(&threads[0].context);          
  if (err) {  
    InterruptsSet(enabled);  
    return ERROR_OTHER;          
  }  
        
  running_thread = &threads[0];  
  InterruptsSet(enabled);          
  return 0;   
          
}          
          
Tid          
ThreadId(void)          
{          
  return running_thread->thread_id;        
}          
        
Tid        
ThreadCreate(void (*f)(void*), void* arg)        
{      
  InterruptsState enabled = InterruptsDisable();    
  free_exited_threads();        
        
  int i = 0;        
    
  while (i < MAX_THREADS && threads[i].state != EMPTY && threads[i].state != EXITED && threads[i].state != KILLED){        
    i++;        
  }        
    
  Tid thread_id = i;     
    
  if (i == MAX_THREADS) {  
    InterruptsSet(enabled);  
    return ERROR_SYS_THREAD;       
  }  
        
  void *sp = malloc(THREAD_STACK_SIZE);       
    
  if (sp == NULL){        
      free(sp);  
      InterruptsSet(enabled);        
      return ERROR_SYS_MEM;        
  }        
    
  int err = getcontext(&(threads[i].context));        
  if (err) {  
    InterruptsSet(enabled);  
    return ERROR_OTHER;  
  }  
    
  threads[i].thread_id = i;        
  threads[i].state = READY;        
  threads[i].sp = sp;        
  // threads[i].exit_code = EXIT_CODE_NORMAL;  
  wait_queues[i].head = NULL;
        
  threads[i].context.uc_mcontext.gregs[REG_RSP] = (unsigned long) (sp + THREAD_STACK_SIZE - 8);
  threads[i].context.uc_mcontext.gregs[REG_RBP] = (unsigned long) sp;
  threads[i].context.uc_mcontext.gregs[REG_RDI] = (unsigned long) f;
  threads[i].context.uc_mcontext.gregs[REG_RSI] = (unsigned long) arg;
  threads[i].context.uc_mcontext.gregs[REG_RIP] = (unsigned long) thread_stub;
    
  insert_into_queue(&rq, &threads[i]);
  InterruptsSet(enabled);
  return thread_id;
}        
        
void        
ThreadExit(ExitCode exit_code)        
{        
  InterruptsState enabled = InterruptsDisable();  
  running_thread->exit_code = exit_code;  
  ThreadWakeAll(&wait_queues[running_thread->thread_id]);  
  remove_from_all_wait_queues(running_thread->thread_id);  
  free_exited_threads();     
    
  if (rq.head == NULL) {       
    running_thread->state = EXITED;  
    running_thread->exit_code = exit_code;   
    InterruptsSet(enabled);     
    exit(exit_code);        
  } 

  running_thread->state = EXITED;        
    
  TCB *next_thread = extract_from_queue(&rq);        
  next_thread->state = RUNNING;        
  running_thread = next_thread;      
    
  int err = setcontext(&(running_thread->context));        
  if (err) {  
    running_thread->exit_code = EXIT_CODE_FATAL;  
    InterruptsSet(enabled);  
    exit(EXIT_CODE_FATAL);  
    }  
  running_thread->exit_code = exit_code;  
  InterruptsSet(enabled);  
}  
        
        
Tid        
ThreadKill(Tid tid)        
{        
  InterruptsState enabled = InterruptsDisable();  
  if (tid < 0 || tid >= MAX_THREADS) {  
    InterruptsSet(enabled);  
    return ERROR_TID_INVALID;  
  }  
  if (tid == running_thread->thread_id) {  
    InterruptsSet(enabled);  
    return ERROR_THREAD_BAD;        
  }  
  if (threads[tid].state == EMPTY || threads[tid].state == KILLED || threads[tid].state == EXITED) {  
    InterruptsSet(enabled);  
    return ERROR_SYS_THREAD;  
  }  
        
  threads[tid].state = KILLED;  
  threads[tid].exit_code = EXIT_CODE_KILL;    
    
  remove_from_queue(&rq, tid);  
  remove_from_all_wait_queues(tid);    
  ThreadWakeAll(&wait_queues[tid]);   
  InterruptsSet(enabled);  
  return tid;        
}        
        
int        
ThreadYield()        
{   
  InterruptsState enabled = InterruptsDisable();     
  free_exited_threads();    
    
  if (rq.head == NULL) {  
    InterruptsSet(enabled);  
    return running_thread->thread_id;    
  }  
    
  int id = running_thread->thread_id;     
    
  volatile int context_called = 0;    
    
  int err = getcontext(&(running_thread->context));        
  if (err) {  
    InterruptsSet(enabled);  
    return ERROR_THREAD_BAD;    
  }  
    
  if (context_called) {     
    context_called = 0;  
    InterruptsSet(enabled);        
    return id;        
    
  } else {        
    
    context_called = 1;     
    
    running_thread->state = READY;      
    insert_into_queue(&rq, running_thread);       
    
    TCB *next_thread = extract_from_queue(&rq);      
    id = next_thread->thread_id;       
  
    
    next_thread->state = RUNNING;        
    running_thread = next_thread;    
    setcontext(&(next_thread->context));           
            
    }         
    InterruptsSet(enabled);  
    return id; 
}        
        
int        
ThreadYieldTo(Tid tid)        
{      
  InterruptsState enabled = InterruptsDisable();  
  free_exited_threads();  
  if (tid < 0 || tid >= MAX_THREADS) {  
    InterruptsSet(enabled);  
    return ERROR_TID_INVALID;  
  }        
  if (tid == running_thread->thread_id) {  
    InterruptsSet(enabled);  
    return tid;  
  }  
  if (threads[tid].state != READY) {  
    InterruptsSet(enabled);  
    return ERROR_THREAD_BAD;  
  }        
      
  volatile int context_called = 0;     
    
  int err = getcontext(&(running_thread->context));        
  if (err) {  
    InterruptsSet(enabled);  
    return ERROR_THREAD_BAD;  
  }       
  // printf("Queue before: ");
  // print_queue(&rq);
  // printf("tid to remove: %d", tid);
  // remove_from_queue(&rq, tid);      
  // print_queue(&rq);
    
   if (context_called) {        
    
    context_called = 0;
    // InterruptsSet(enabled);        
    // return running_thread->thread_id;         
    
   } else {        
    
    context_called = 1;        
    
    running_thread->state = READY;        
    insert_into_queue(&rq, running_thread);
    remove_from_queue(&rq, tid);             
    
    TCB *next_thread = &threads[tid];        
    next_thread->state = RUNNING;        
    
    running_thread = next_thread;    
    setcontext(&(running_thread->context));    
    }        
  InterruptsSet(enabled);  
  return tid;        
        
}       
  
WaitQueue*  
WaitQueueCreate(void)  
{  
  InterruptsState enabled = InterruptsDisable();  
  WaitQueue *queue = malloc(sizeof(WaitQueue));  
  queue->head = NULL;  
  InterruptsSet(enabled);  
  return queue;  
}  
  
int  
WaitQueueDestroy(WaitQueue* queue)  
{  
  InterruptsState enabled = InterruptsDisable();  
  if (queue->head != NULL) {  
    InterruptsSet(enabled);  
    return ERROR_OTHER;  
  }  
  free(queue);  
  InterruptsSet(enabled);  
  return 0;  
}  
  
void  
ThreadSpin(int duration)  
{  
  struct timeval start, end, diff;  
  
  int ret = gettimeofday(&start, NULL);  
  assert(!ret);  
  
  while (1) {  
    ret = gettimeofday(&end, NULL);  
    assert(!ret);  
    timersub(&end, &start, &diff);  
  
    if ((diff.tv_sec * 1000000 + diff.tv_usec) >= duration) {  
      return;  
    }  
  }  
}  
  
int  
ThreadSleep(WaitQueue* queue)  
{  
  InterruptsState enabled = InterruptsDisable();  
  assert(queue != NULL);  
  free_exited_threads();    
    
  if (rq.head == NULL) {  
    InterruptsSet(enabled);  
    return ERROR_SYS_THREAD;  
  }  
    
  int id = rq.head->thread->thread_id;     
    
  volatile int context_called = 0;    
    
  int err = getcontext(&(running_thread->context));        
  if (err) {  
    InterruptsSet(enabled);  
    return ERROR_THREAD_BAD;    
  }  
    
    
  if (context_called) {     
    context_called = 0;  
    InterruptsSet(enabled);        
    return id;        
    
  } else {    
    
    context_called = 1;     
    
    running_thread->state = BLOCKED;  
  
    insert_into_queue(queue, running_thread);  
  
    TCB *next_thread = extract_from_queue(&rq);      
    id = next_thread->thread_id;        
    
    next_thread->state = RUNNING;        
    running_thread = next_thread;    
    setcontext(&(next_thread->context));
    InterruptsSet(enabled);  
    return id;          
            
    }
    InterruptsSet(enabled);  
    return id;     
}  
  
int  
ThreadWakeNext(WaitQueue* queue)  
{  
  InterruptsState enabled = InterruptsDisable();  
  assert(queue != NULL);  
  if (queue->head == NULL)   
  {  
    InterruptsSet(enabled);  
    return 0;  
  }  
  TCB *next_thread = extract_from_queue(queue);  
  insert_into_queue(&rq, next_thread);  
  next_thread->state = READY;  
  InterruptsSet(enabled);  
  return 1;  
}  
  
int  
ThreadWakeAll(WaitQueue* queue)  
{  
  InterruptsState enabled = InterruptsDisable();  
  assert(queue != NULL);  
  if (queue->head == NULL)   
  {  
    InterruptsSet(enabled);  
    return 0;  
  }  
  TCB *curr_thread;  
  int count = 0;  
  while (queue->head != NULL) {  
    curr_thread = extract_from_queue(queue);  
    insert_into_queue(&rq, curr_thread);  
    curr_thread->state = READY;  
    count++;   
  }  
  InterruptsSet(enabled);  
  return count;  
}  
  
int  
ThreadJoin(Tid tid, int* exit_code)  
{  
  InterruptsState enabled = InterruptsDisable();  
  
  if (tid < 0 || tid >= MAX_THREADS) {  
    InterruptsSet(enabled);  
    return ERROR_TID_INVALID;  
  }  
  // *exit_code = threads[tid].exit_code;  
  if (threads[tid].state == EMPTY || threads[tid].state == KILLED || threads[tid].state == EXITED) {
    InterruptsSet(enabled);
    return ERROR_SYS_THREAD;
  }  
  
  if (tid == running_thread->thread_id) {
    InterruptsSet(enabled);
    return ERROR_THREAD_BAD;
  }  
   
  ThreadSleep(&wait_queues[tid]);
  if (exit_code != NULL) *exit_code = threads[tid].exit_code;
  
  InterruptsSet(enabled);
  return tid;
}  
