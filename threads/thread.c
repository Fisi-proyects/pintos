#include "threads/thread.h"
#include "threads/arit_operations.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/** Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/** List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* lista de todos los procesos durmientes (blockeados) espaerando
  a ser llamados mediante wake up */
static struct list sleep_thread_list;

/** List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

/** Idle thread. */
static struct thread *idle_thread;

/** Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/** Lock used by allocate_tid(). */
static struct lock tid_lock;

/** Stack frame for kernel_thread(). */
struct kernel_thread_frame 
  {
    void *eip;                  /**< Return address. */
    thread_func *function;      /**< Function to call. */
    void *aux;                  /**< Auxiliary data for function. */
  };

/** Statistics. */
static long long idle_ticks;    /**< # of timer ticks spent idle. */
static long long kernel_ticks;  /**< # of timer ticks in kernel threads. */
static long long user_ticks;    /**< # of timer ticks in user programs. */

/** Scheduling. */
#define TIME_SLICE 4            /**< # of timer ticks to give each thread. */
static unsigned thread_ticks;   /**< # of timer ticks since last yield. */

int load_avg; // variable global para el load_avg

/** If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void thread_schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);

/** Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void) 
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&ready_list);
  list_init (&all_list);

  list_init (&sleep_thread_list); // inicializando lista de threads dormidos

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();

  initial_thread->nice = 0;
  initial_thread->recent_cpu = 0;

}

bool compare_thread_priority(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED){
  struct thread *thread_a = list_entry(a, struct thread, elem);
  struct thread *thread_b = list_entry(b, struct thread, elem);
  return thread_a->priority > thread_b->priority;

}

/** Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) 
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started); 

  load_avg = 0; // inicializando el load_avg

  load_avg = 0; // inicializando el load_avg

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

/** Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void) 
{
  struct thread *t = thread_current ();

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;

  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE)
    intr_yield_on_return ();
}

/** Prints thread statistics. */
void
thread_print_stats (void) 
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/** Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

  /* Add to run queue. */
  thread_unblock (t);


  thread_preempt();


  return tid;
}

/** Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void) 
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}

/** Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t) 
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
  //ordenando por prioridad en lugar de push_back
  list_insert_ordered(&ready_list, &t->elem, compare_thread_priority, NULL);
  t->status = THREAD_READY;
  intr_set_level (old_level);
}

/** Returns the name of the running thread. */
const char *
thread_name (void) 
{
  return thread_current ()->name;
}

/** Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) 
{
  struct thread *t = running_thread ();
  
  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/** Returns the running thread's tid. */
tid_t
thread_tid (void) 
{
  return thread_current ()->tid;
}

/** Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) 
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  process_exit ();
#endif

  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it calls thread_schedule_tail(). */
  intr_disable ();
  list_remove (&thread_current()->allelem);
  thread_current ()->status = THREAD_DYING;
  schedule ();
  NOT_REACHED ();
}

/** Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) 
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;
  
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  if (cur != idle_thread) 

    list_insert_ordered(&ready_list, &cur->elem, compare_thread_priority, 0);

  cur->status = THREAD_READY;
  schedule ();//cede el cpu al siguiente hilo en la lista
  intr_set_level (old_level);
}


void thread_preempt (void)
{
  //si la lista de hilos listos esta vacia
  if (list_empty (&ready_list)) { return; }
  //si el hilo actual tiene menor prioridad que el hilo con mayor prioridad
  //se cede el cpu al hilo con mayor prioridad
  struct thread *current_thread = thread_current ();
  struct thread *next_thread = list_entry (
    list_front (&ready_list), struct thread, elem
  );
  //si el hilo actual tiene menor prioridad que el hilo con mayor prioridad
  //se cede el cpu al hilo con mayor prioridad
  if (current_thread->priority < next_thread->priority) {
    thread_yield (); 
  }
}


/* Start of timer_sleep alarm clock implementation */

bool compare_to_wake_up(const struct list_elem *a, const struct list_elem *b, void *aux){
  struct thread *thread_a = list_entry(a, struct thread, elem);
  struct thread *thread_b = list_entry(b, struct thread, elem);
  return thread_a->remain_time_to_wake_up < thread_b->remain_time_to_wake_up;  //comparamos los tiempos de espera de los threads
  //si el tiempo de espera de a es menor que el tiempo de espera de b
  //devolvemos true, de lo contrario devolvemos false
  //esto se hace para ordenar la lista de threads dormidos
  //de menor a mayor tiempo de espera
  //para que el thread que deba despertar antes sea el primero
  //en la lista
  //es una especie de ordenamiento burbuja
}

void set_thread_sleep(int64_t ticks){
  //desactivamos las interrupciones
  //para evitar que se llame a esta funcion
  //mientras se esta ejecutando
  enum intr_level old_level = intr_disable ();
  struct thread *current_thread = thread_current ();

  ASSERT (is_thread (current_thread));
  ASSERT (current_thread != idle_thread);

  current_thread->remain_time_to_wake_up = ticks;
  
      //guardamos el tiempo que necesita esperar el  thread
  list_insert_ordered(&sleep_thread_list, &current_thread->elem, compare_to_wake_up, NULL); //insertamos el thread en la lista de threads dormidos
  thread_block(); //bloqueamos el thread // hasta que el tiempo de espera se cumpla
  intr_set_level (old_level); //activamos las interrupciones
}

void wake_up_thread(int64_t ticks){
    // Despertar hilos dormidos cuyo tiempo de despertar haya llegado
    while (!list_empty(&sleep_thread_list)) {
        struct thread *t = list_entry(list_front(&sleep_thread_list), struct thread, elem);

        if (t->remain_time_to_wake_up <= ticks) {
            list_pop_front(&sleep_thread_list);
            thread_unblock(t);
        } else {
            break;  // Como la lista está ordenada, si el primero no debe despertarse, ninguno más debe
          }
    }
}

/* end of the thread_sleep implementation */



void thread_anticipate(void){
  //anticipar el hilo con mayor prioridad
  //si el hilo actual tiene menor prioridad
  //que el hilo con mayor prioridad
  //se cede el cpu al hilo con mayor prioridad
  if (list_empty(&ready_list)) return;
  struct thread *current_thread = thread_current();
  struct thread *next_thread = list_entry(list_front(&ready_list), struct thread, elem);

  if (current_thread->priority < next_thread->priority) {
    thread_yield();
  }
}
void donate_priority (void)
{
  int depth = 0;
  struct thread *current_thread = thread_current ();
  
  for (depth = 0; current_thread->released_lock != NULL && depth < 8; ++depth, current_thread = current_thread->released_lock->holder) {
    if (current_thread->released_lock->holder->priority < current_thread->priority) {
      current_thread->released_lock->holder->priority = current_thread->priority;
    }
  }
}

void update_priority (void)
{
  struct thread *current_thread = thread_current ();
  int new_priority = current_thread->original_priority;
  if(!list_empty (&current_thread->donations)){
    struct thread *top_donor= list_entry(list_front(&current_thread->donations),struct thread,donation_elem) ;
    current_thread->priority=(new_priority<top_donor)? top_donor->priority: new_priority;
  }
  else{
    current_thread->priority = new_priority;
  }
}

void remove_threads_from_donations (struct lock *lock)
{
  struct thread *current_thread = thread_current ();
  struct list_elem *e;

  for (e = list_begin (&current_thread->donations); e != list_end (&current_thread->donations);) {
    if(list_entry (e, struct thread, donation_elem)->released_lock == lock) {
      e = list_remove (e);
    } else {
      e = list_next (e);
    }

  }
}

/** Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF);

  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, allelem);
      func (t, aux);
    }
}

//donar prioridad
void donate_priority(void){
  int i = 0;
  struct thread *current_thread = thread_current ();
  struct lock *lock = current_thread->released_lock;
  struct thread *holder = lock->holder;

  for(i = 0;lock!= NULL && i < 8; i++, current_thread = holder){
    if(holder->priority < current_thread->priority){
      holder->priority = current_thread->priority;
    }
  }
}

//actualizar prioridad
void update_priority(void){
  struct thread *current_thread = thread_current ();
  int max_priority = current_thread->original_priority;
  int new_priority;

  if (!list_empty(&current_thread->donations)) {
    new_priority = list_entry(list_max(&current_thread->donations, compare_thread_priority, NULL), struct thread, donation_elem)->priority;
    max_priority = new_priority > max_priority ? new_priority : max_priority;
  }

  current_thread->priority = max_priority;
}

//remover thread de las donaciones
void remove_threads_from_donations(struct lock *lock){
  struct thread *current_thread = thread_current ();
  struct list_elem *e;
  while(!list_empty(&current_thread->donations)){
    if(list_entry(e, struct thread, donation_elem)->released_lock == lock){
      e = list_remove(e);
    } else {
      e = list_next(e);
    }
  }
}

/** Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority) 
{
  //si el scheduler mlfqs esta activado
  //no se puede cambiar la prioridad
  if(thread_mlfqs) {return;};
  struct thread *current_thread = thread_current();
  
  //si la nueva prioridad es la misma que la prioridad anterior 
  //no se hace nada
  if(current_thread->priority == new_priority){
    return;
  }
  //se guardan ambas prioridades
  current_thread->original_priority = new_priority;
  //se actualiza la prioridad
  update_priority();
  //se cede el cpu al hilo con mayor prioridad
  thread_preempt();

}

/** Returns the current thread's priority. */
int
thread_get_priority (void) 
{
  return thread_current ()->priority;
}

//funcion para calcular la prioridad de un hilo
void bsd_priority(struct thread *t){

  if(t == idle_thread) {return;};

  t->priority = PRI_MAX - fixed_point_to_int_round(divide_fixed_point_and_int(t->recent_cpu, 4)) - (t->nice * 2);
  if (t->priority > PRI_MAX) {
    t->priority = PRI_MAX;
  }
  if (t->priority < PRI_MIN) {
    t->priority = PRI_MIN;
  }
}

//actualizar prioridad
void bsd_update_priority(void){
  struct list_elem *e;
  for(e = list_begin(&all_list); e != list_end(&all_list); e = list_next(e)){
    struct thread *t = list_entry(e, struct thread, allelem);
    if (t != idle_thread) {
      bsd_priority(t);
    }
  }
  list_sort(&ready_list, compare_thread_priority, NULL);
}

//funcion para calcular el recent_cpu de un hilo
void bsd_recent_cpu(struct thread *t){
  int load_avg_2 = multiply_fixed_point_and_int(load_avg, 2);
  int decay = divide_fixed_points(load_avg_2, add_fixed_point_and_int(load_avg_2, 1));
  t->recent_cpu = add_fixed_point_and_int(multiply_fixed_points(decay, t->recent_cpu), t->nice);
}

//actualizar recent_cpu
void bsd_update_recent_cpu(void){
  struct list_elem *e;
  for(e = list_begin(&all_list); e != list_end(&all_list); e = list_next(e)){
    struct thread *t = list_entry(e, struct thread, allelem);
    if(t != idle_thread){
      bsd_recent_cpu(t);
    }
  }
}

//Aumentar en 1 el recent_cpu del hilo actual
void bsd_recent_cpu_increment(void){
  struct thread *current_thread = thread_current ();
  if(current_thread != idle_thread){
    current_thread->recent_cpu = add_fixed_point_and_int(thread_current ()->recent_cpu, 1);
  }
}

//Calcular el load_avg
void bsd_load_avg(void){
  int ready_threads = list_size(&ready_list);
  if(thread_current() != idle_thread){
    ready_threads++;
  }
  load_avg = divide_fixed_point_and_int(add_fixed_points(multiply_fixed_point_and_int(load_avg,59),int_to_fixed_point(ready_threads)),60);
}

/** Sets the current thread's nice value to NICE. */
void
thread_set_nice (int nice UNUSED) 
{
  enum intr_level old_level = intr_disable ();
  struct thread *current_thread = thread_current ();
  current_thread->nice = nice;
  bsd_priority(current_thread);
  list_sort(&ready_list, compare_thread_priority, NULL);
  if(current_thread != idle_thread){

    thread_anticipate();

  }
  intr_set_level (old_level);
}

/** Returns the current thread's nice value. */
int
thread_get_nice (void) 
{
  enum intr_level old_level = intr_disable ();
  int nice = thread_current ()->nice;
  intr_set_level (old_level);
  return nice;
}

/** Returns 100 times the system load average. */
int
thread_get_load_avg (void) 
{
  enum intr_level old_level = intr_disable ();
  int load_avg_100 = fixed_point_to_int_round(multiply_fixed_point_and_int(load_avg, 100));
  intr_set_level (old_level);
  return load_avg_100;
}

/** Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) 
{
  enum intr_level old_level = intr_disable ();
  int recent_cpu = fixed_point_to_int_round(multiply_fixed_point_and_int(thread_current ()->recent_cpu, 100));
  intr_set_level (old_level);
  return recent_cpu;
}

/** Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) 
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;) 
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/** Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) 
{
  ASSERT (function != NULL);

  intr_enable ();       /**< The scheduler runs with interrupts off. */
  function (aux);       /**< Execute the thread function. */
  thread_exit ();       /**< If function() returns, kill the thread. */
}

/** Returns the running thread. */
struct thread *
running_thread (void) 
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/** Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/** Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
  enum intr_level old_level;

  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;
  t->original_priority = priority;//guardamos la prioridad original
  t->released_lock = NULL;//inicializamos el lock que el hilo ha liberado
  t->magic = THREAD_MAGIC;

  t->nice = 0;
  t->recent_cpu = 0;

  list_init(&t->donations);

  old_level = intr_disable ();
  list_push_back (&all_list, &t->allelem);
  intr_set_level (old_level);
}

/** Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size) 
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/** Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void) 
{
  if (list_empty (&ready_list))
    return idle_thread;
  else
    return list_entry (list_pop_front (&ready_list), struct thread, elem);
}

/** Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
thread_schedule_tail (struct thread *prev)
{
  struct thread *cur = running_thread ();
  
  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread) 
    {
      ASSERT (prev != cur);
      palloc_free_page (prev);
    }
}

/** Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
static void
schedule (void) 
{
  struct thread *cur = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next)
    prev = switch_threads (cur, next);
  thread_schedule_tail (prev);
}

/** Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) 
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}

/** Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);