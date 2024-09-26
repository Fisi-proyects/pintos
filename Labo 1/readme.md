# Laboratorio 1

## Grupo

- Jeremy Rosillo Ramirez: <jeremy.rosillo@unmsm.edu.pe>
- Sebastian Cueto Salazar: <sebastian.cueto@unmsm.edu.pe>
- Riccardo Calderon Flores: <ricardo.calderon4@unmsm.edu.pe>

---

## Preliminaries

- <https://pkuflyingpig.gitbook.io/pintos/project-description/lab1-threads>
- <http://web.stanford.edu/class/cs140/projects/pintos/pintos_2.html#SEC23>
- <https://youtu.be/myO2bs5LMak?si=-NtvCYqRJEMwOHKT>
- <https://youtu.be/4-OjMqyygss?si=brRxwhv9nAWWPS4m>
  
---

## Alarm Clock

### Data Structures

#### 1. Sleep_list

Utilizaremos esta lista para guardar a todos los hilos durmientes provenientes de `time_sleep()` en estado  `THREAD_BLOCKED`

```c
static struct list sleep_thread_list;
```

#### 2. remain_time_to_wake_up

Para mejorar el temporizador, necesitamos almacenar el tiempo en ticks para cada hilo. Este tiempo representará la duración durante la cual el hilo estará dormido.

```c
struct thread {
    /*  */
  int64_t remain_time_to_wake_up;  // Tiempo .
    /*  */
};
```

### Algorithms

#### 1. Timer_Sleep

El proceso de poner un hilo a dormir usando timer_sleep() implica bloquear el hilo y añadirlo a una lista de hilos dormidos ordenada. En cada interrupción de timer, se verifica si algún hilo ha completado su tiempo de sueño para desbloquearlo.

```c

timer_sleep (int64_t ticks) 
{
  int64_t start = timer_ticks (); // 
    if(ticks <=0)return;
  ASSERT (intr_get_level () == INTR_ON);
  set_thread_sleep(ticks+start);
}

```

Si los ticks son menores o iguales a cero regresa la funcion y no se ejecuta.

- ##### set_thread_sleep

    La función `set_thread_sleep()` desactiva las interrupciones, ajusta el tiempo de despertar del hilo y lo inserta en la lista de hilos dormidos en el lugar correcto. Luego, bloquea el hilo hasta que se despierte.

```c
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
  list_insert_ordered(&sleep_thread_list, &current_thread->elem, compare_to_wake_up, 0); //insertamos el thread en la lista de threads dormidos
  thread_block(); //bloqueamos el thread // hasta que el tiempo de espera se cumpla
  intr_set_level (old_level); //activamos las interrupciones
}
```

- ##### compare_to_wake_up

El propósito de esta función es ordenar una lista de hilos dormidos en función del tiempo que les queda para despertarse.

```c
bool compare_to_wake_up(const struct list_elem *a, const struct list_elem *b, void *aux){    struct thread *thread_a = list_entry(a, struct thread, elem);
    struct thread *thread_b = list_entry(b, struct thread, elem);
    return thread_a->remain_time_to_wake_up < thread_b->remain_time_to_wake_up;  //comparamos los tiempos de espera de los threads
}
```

- ##### Wake_up_thread

`wake_up_thread()` se ejecuta en cada interrupción de temporizador y revisa si algún hilo ha terminado su periodo de sueño. Si es así, se desbloquea y se elimina de la lista de hilos dormidos y pasa a la listo de hilos listos para su ejecucion.

```c
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

```

- #### timer_interrupt
  
  La función `timer_interrupt` es una rutina de interrupción que se ejecuta cada vez que ocurre una interrupción del temporizador. Incrementa una variable global ticks, llama a la función `thread_tick` para manejar el tick del hilo actual y luego llama a `wake_up_thread` pasando el número de ticks actuales para despertar hilos que puedan estar esperando.

```c
    static void
timer_interrupt (struct intr_frame *args UNUSED)
{
  //set time_to w
  ticks++;
  thread_tick ();
  wake_up_thread(ticks);
}
```

##### Pasos tomados para minimizar el tiempo pasado dentro de `timer_interrupt`

Para minimizar el tiempo en el manejador de interrupciones, se realiza solo el trabajo mínimo necesario: incrementar el contador, manejar el tick del hilo y despertar los hilos. A comparacion de la anterior optimizacion los hilos ya no tienen que esperar forzosamente a que pase el numero de interrupciones por segundo.En su lugar se optimiza creando un atributo de ticks locales y mejorando la carga utilizando una lista de threads dormidos

### Synchronization

###### ¿Cómo se evitan las condiciones de carrera cuando varios hilos llaman a timer_sleep() simultáneamente?

Para evitar condiciones de carrera cuando múltiples hilos llaman a `timer_sleep()` al mismo tiempo, se desactivan las interrupciones durante la inserción de los hilos en la lista de dormidos y al procesar la lista durante las interrupciones del temporizador. Esto asegura que no haya conflictos mientras se actualizan las estructuras de los hilos.

```c
  //Desactivamos interrupciones
  enum intr_level old_level = intr_disable ();
  /* do things */
  intr_set_level (old_level); 
  //activamos interrupciones
```

###### ¿Cómo se evitan las condiciones de carrera cuando ocurre una interrupción de temporizador durante una llamada a timer_sleep()

Si ocurre una interrupción de temporizador mientras se está ejecutando `timer_sleep()`, las interrupciones ya estarán deshabilitadas por `intr_disable()`. Esto evita que haya interferencia mientras se manipula la lista de hilos dormidos.

### Rationale

###### ¿Por que escogimos este diseño?¿De que manera es superior a la anterior implementacion?

Este diseño fue elegido porque optimiza la eficiencia en el manejo de los hilos dormidos. Al usar una lista ordenada, el tiempo que se invierte en procesar los hilos dormidos en cada tick se minimiza. Comparado con un enfoque no ordenado o con búsquedas lineales, este método reduce la carga de procesamiento y hace que el temporizador sea más eficiente.

---

## Priority Scheduling

### Data Structures

#### 1. Cambios en struct thread

- `donations`: Lista de hilos que han donado su prioridad a este hilo.
- `donation_elem`: Elemento de lista para insertar el hilo en la lista donations de otro hilo.
- `released_lock`: Puntero a la cerradura que el hilo ha liberado.
- `original_priority`: Sirve para guardar la prioridad original en caso de donacion de prioridad.

```c
struct thread {
  /*  */
  struct lock * released_lock;
  struct list donations;
  struct list donatios_elem;
  int original_priority;
  /*  */
}
```

#### 2. Donacion de prioridad

```
Inicialmente:
  Prioridades:  A (10), B (20), C (30)

  Hilo A adquiere el lock:
  A (10) [lock]

  Hilo B intenta adquirir el lock y se bloquea:
  B (20) -> A (20) [lock]

  Hilo C intenta adquirir el lock y se bloquea:
  C (30) -> A (30) [lock]

Representación en ASCII Art:

  +---------+         +---------+         +---------+
  | Thread  |         | Thread  |         | Thread  |
  |    A    |         |    B    |         |    C    |
  | Priority|         | Priority|         | Priority|
  |   10    |         |   20    |         |   30    |
  +---------+         +---------+         +---------+
       |                   |                   |
       v                   |                   |
  +---------+              |                   |
  | Lock    |              |                   |
  | Holder  |              |                   |
  |   A     |              |                   |
  | Priority|              |                   |
  |   10    |              |                   |
  +---------+              |                   |
       |                   |                   |
       v                   v                   |
  +---------+         +---------+              |
  | Thread  |         | Thread  |              |
  |    A    |         |    B    |              |
  | Priority|         | Priority|              |
  |   20    |         |   20    |              |
  +---------+         +---------+              |
       |                   |                   |
       v                   v                   v
  +---------+         +---------+         +---------+
  | Thread  |         | Thread  |         | Thread  |
  |    A    |         |    B    |         |    C    |
  | Priority|         | Priority|         | Priority|
  |   30    |         |   20    |         |   30    |
  +---------+         +---------+         +---------+
```

**Explicacion**

1. **Inicialmente**:

- Hilo A tiene prioridad 10.
- Hilo B tiene prioridad 20.
- Hilo C tiene prioridad 30.

2. **Hilo A adquiere el lock**:

- Hilo A está ejecutándose y adquiere el lock.

3. **Hilo B intenta adquirir el lock**:

- Hilo B se bloquea intentando adquirir el lock y dona su prioridad (20) a Hilo A.
- La prioridad de Hilo A se eleva a 20.

4. **Hilo C intenta adquirir el lock**:

- Hilo C se bloquea intentando adquirir el lock y dona su prioridad (30) a Hilo A.
- La prioridad de Hilo A se eleva a 30.

### Algorithms

#### 1. Compare thread priority

La función `compare_thread_priority` en el archivo thread.c compara la prioridad de dos hilos (`thread_a` y `thread_b`) obtenidos a partir de elementos de una lista (a y b), y devuelve true si la prioridad de `thread_a` es mayor que la de `thread_b`, lo que permite ordenar hilos en una lista basada en su prioridad.Estos threads posteriormente se insertaran de forma ordenada en `ready_list`.

```c
bool compare_thread_priority(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED){
  struct thread *thread_a = list_entry(a, struct thread, elem);
  struct thread *thread_b = list_entry(b, struct thread, elem);
  return thread_a->priority > thread_b->priority;
}
```

#### 2. Compare semaphore priority

La función `sema_priority` compara la prioridad de los hilos que están esperando en dos semáforos diferentes, devolviendo `true` si la prioridad del hilo en el primer semáforo es mayor que la del hilo en el segundo semáforo, lo que permite ordenar semáforos en una lista basada en la prioridad de los hilos que están esperando en ellos.

```c
bool compare_sema_priority (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED)
{
  return (sema_waiters_head_thread_priority (&(list_entry (a, struct semaphore_elem, elem)->semaphore)))
    > (sema_waiters_head_thread_priority (&(list_entry (b, struct semaphore_elem, elem)->semaphore)));
}
```

- ##### sema_waiters_head_thread_priority

  La función `semaphore_waiters_front_thread_priority` se utiliza para obtener la prioridad del hilo que está al frente de la lista de espera de un semáforo. Esta funcion se utiliza en `compare_sema_priority` para poder comparar los waiters de los semaforos.

```c
  //obtener la prioridad del hilo con mayor prioridad en la lista de waiters
int semaphore_waiters_front_thread_priority(struct semaphore *sema){
  if (list_empty(&sema->waiters)) return PRI_MIN - 1;
  struct thread *front_thread = list_entry(list_front(&sema->waiters), struct thread, elem);
  return front_thread->priority;
}
```

#### 3. Preempt Thread

La función verifica si el hilo en ejecución debe ser interrumpido (preempted) para dar paso a un hilo de mayor prioridad que esté en la lista de listos para ejecutarse `ready_list`.

```c
  void thread_preempt (void){

  if (list_empty (&ready_list)) { return; }

  struct thread *current_thread = thread_current ();
  struct thread *next_thread = list_entry (
    list_front (&ready_list), struct thread, elem
  );

  if (current_thread->priority < next_thread->priority) {
    thread_yield (); 
  }
}

```

- ##### Cuando se crea un thread `thread_create()`
  
  En `thread_create`, se llama a `preempt_thread` para asegurarse de que, después de crear un nuevo hilo, si este hilo tiene una mayor prioridad que el hilo actualmente en ejecución, el sistema ceda el cpu y le dé el control al nuevo hilo.

```c
tid_t thread_create (/* parameters */) {
  /* 
    do things
   */

  /* Add to run queue. */
  thread_unblock (t);

  thread_preempt();

  return tid;
}
```

- ##### Cuando se modifica la prioridad `thread_set_priority()`

La llamada a `thread_preempt` en `thread_set_priority` asegura que, después de cambiar la prioridad de un hilo, el sistema operativo reevalúe cuál hilo debe ejecutarse a continuación. Si hay un hilo con mayor prioridad listo para ejecutarse, `thread_preempt` fuerza la interrupción del hilo actual para ceder el procesador al hilo de mayor prioridad, manteniendo así la eficiencia y la equidad en la planificación de hilos.

```c
void
thread_set_priority (int new_priority) 
{ 
  /* 
    do things
   */
  //se cede el cpu al hilo con mayor prioridad
  thread_preempt();

}
```

#### 4. Insercion por Prioridades

Cuando se desbloquea un thread de la lista de threads dormidos mediante `thread_unblock` estos se guardan de forma ordenada comparando su prioridad en `ready_list`.

```c
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
```

El mismo caso para `thread_yield`.El thread que cede la cpu pasa a la `ready_list` guardandose segun la prioridad.

```c
void
thread_yield (void) 
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;
  
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  if (cur != idle_thread) 
    //guardamos el thread de forma ordenada en ready list
    list_insert_ordered(&ready_list, &cur->elem, compare_thread_priority, 0);

  cur->status = THREAD_READY;
  schedule ();//cede el cpu al siguiente hilo en la lista
  intr_set_level (old_level);
}
```

#### 4. Semaforos

- ##### Sema Down

`sema_down` Añade el thread actual a la lista de espera del semáforo de forma ordenada por prioridad y lo bloquea si el valor del semáforo es 0.

```c
void sema_down (struct semaphore *sema) {
  enum intr_level old_level;

  ASSERT (sema != NULL);
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  while (sema->value == 0) 
    {

      //list_push_back (&sema->waiters, &thread_current ()->elem);
      list_insert_ordered(&sema->waiters, &thread_current()->elem, compare_thread_priority, NULL);

      thread_block ();
    }
  sema->value--;
  intr_set_level (old_level);
}
```

- ##### Sema Up
  
`sema_up` Incrementa el valor del semáforo y despierta al hilo de mayor prioridad en la lista de espera si hay hilos esperando.Antes de despertar el thread de la lista de espera se reordenan mediante `list_sort`.Ademas debido a que la prioridad de los threads puede cambiar se llama a `preempt_thread` al final de la funcion para reevaluar las prioridades.

```c
void
sema_up (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);

  old_level = intr_disable ();

  if (!list_empty (&sema->waiters)) {
    list_sort(&sema->waiters, compare_thread_priority, NULL);

    thread_unblock (list_entry (list_pop_front (&sema->waiters),
                                struct thread, elem));
  }
  sema->value++;


  thread_preempt();

  intr_set_level (old_level);
}

```

#### 5. Conditions

- ##### Cond Wait
  
  Pone un hilo en espera en una variable de condición, añadiéndolo a la lista de espera de forma ordenada por prioridad, liberando el candado asociado y esperando en un semáforo.

  ```c
  void cond_wait (struct condition *cond, struct lock *lock) {
    struct semaphore_elem waiter;

    ASSERT (cond != NULL);
    ASSERT (lock != NULL);
    ASSERT (!intr_context ());
    ASSERT (lock_held_by_current_thread (lock));
  
    sema_init (&waiter.semaphore, 0);

  
    list_insert_ordered(&cond->waiters, &waiter.elem,         compare_sema_priority, NULL);
  
    //list_push_back (&cond->waiters, &waiter.elem);

    lock_release (lock);
    sema_down (&waiter.semaphore);
    lock_acquire (lock);
  }
  ```

- ##### Cond_signal

Señala (despierta) al hilo de mayor prioridad que está esperando en una variable de condición, ordenando la lista de espera y señalando el semáforo del thread.

```c
void cond_signal (struct condition *cond, struct lock *lock UNUSED) {
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));


  if (!list_empty (&cond->waiters)) {
    list_sort(&cond->waiters,compare_sema_priority, NULL);

    sema_up (&list_entry (list_pop_front (&cond->waiters),
                          struct semaphore_elem, elem)->semaphore);
  }
}
```

  #### Donacion de Prioridad

  Inicializamos los atributos necesarios para la implementacion de las donaciones en `thread_init`;

  ```c
  static void
init_thread (struct thread *t, const char *name, int priority){
  /*...*/
  t->original_priority = priority;
  t->released_lock = NULL;

  list_init (&t->donations);
  /*...*/
}
  ```

- ##### Lock Acquire

  La función lock_acquire se utiliza para adquirir un candado. Si el candado ya está en uso, el hilo actual puede donar su prioridad al hilo que posee el candado para evitar la inversión de prioridades.

  ```c
  void lock_acquire(struct lock *lock) {
    struct thread *current_thread = thread_current();

    if (lock->holder != NULL && !thread_mlfqs) {
        current_thread->waiting_for_lock = lock;
        donate_priority();
    }

    sema_down(&lock->semaphore);
    current_thread->waiting_for_lock = NULL;
    lock->holder = current_thread;
    list_push_back(&current_thread->locks, &lock->elem);
    }
  ```

- ##### Lock Release

  La función `lock_release` se utiliza para liberar un candado. Cuando se libera un candado, se debe actualizar la prioridad del hilo que lo libera.

  ```c
  void lock_release(struct lock *lock) {
    struct thread *current_thread = thread_current();

    list_remove(&lock->elem);
    if (!thread_mlfqs) {
        remove_threads_from_donations(lock);
        update_priority();
    }

    lock->holder = NULL;
    sema_up(&lock->semaphore);
  }
  ```

- ##### Donate priority

  La función `donate_priority` se utiliza para donar la prioridad del thread actual al thread que posee el candado que está esperando.

  ```c
    void donate_priority(void) {
    struct thread *current_thread = thread_current();
    struct lock *lock = current_thread->waiting_for_lock;
    int depth = 0;

    while (lock != NULL && depth < 8) {
        if (lock->holder == NULL || lock->holder->priority >= current_thread->priority) {
            break;
        }

        lock->holder->priority = current_thread->priority;
        current_thread = lock->holder;
        lock = current_thread->waiting_for_lock;
        depth++;
    }
  }
  ```

- ##### remove thread from donations
  
  Se utiliza para eliminar los threads que ya no están esperando por un candado de la lista de donaciones.  

- ##### update priority
  
  Actualiza la prioridad del hilo actual después de liberar un candado. Primero, restaura la prioridad original del hilo. Luego, si la lista de donaciones no está vacía, ordena la lista por prioridad y actualiza la prioridad del hilo actual si el donante de mayor prioridad tiene una prioridad mayor

  ```c
  void update_priority(void) {
    struct thread *current_thread = thread_current();
    current_thread->priority = current_thread->original_priority;

    if (!list_empty(&current_thread->donations)) {
        list_sort(&current_thread->donations, thread_priority_comparator, NULL);
        struct thread *highest_priority_donor = list_entry(list_front(&current_thread->donations), struct thread, donation_elem);
        if (highest_priority_donor->priority > current_thread->priority) {
            current_thread->priority = highest_priority_donor->priority;
        }
    }
  }
  ```

### SYNCHRONIZATION

##### Describe una posible condición de carrera en `thread_set_priority()` y explica cómo tu implementación la evita. ¿Puedes usar un cerrojo (lock) para evitar esta condición de carrera?

Una posible condición de carrera en la función `thread_set_priority` ocurre cuando múltiples hilos intentan cambiar la prioridad de un hilo al mismo tiempo. Esto puede llevar a inconsistencias en la prioridad del hilo, ya que las operaciones de lectura y escritura en la variable de prioridad no son atómicas.

En la implementación actual de `thread_set_priority`, la condición de carrera se evita deshabilitando las interrupciones durante la operación de cambio de prioridad. Esto asegura que la operación sea atómica y que ningún otro hilo pueda interferir mientras se está cambiando la prioridad del hilo actual.

```c
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
```
### RATIONALE

##### ¿Por qué elegiste este diseño? ¿En qué aspectos es superior a otro diseño que consideraste?

  Este diseño fue elegido porque asegura la equidad y eficiencia en la planificación de threads mediante la deshabilitación de interrupciones y la donación de prioridades. La deshabilitación de interrupciones en `thread_set_priority` garantiza que las operaciones críticas sean atómicas, evitando condiciones de carrera sin la sobrecarga de cerrojos adicionales. La donación de prioridades en `lock_acquire` y `donate_priority` evita la inversión de prioridades, asegurando que los hilos de alta prioridad no sean bloqueados por hilos de baja prioridad. Además, la gestión adecuada de las prioridades en `lock_release`, `remove_threads_from_donations` y update_priority mantiene la consistencia y relevancia de las prioridades de los hilos.

---

## Advanced Scheduler

### Data Structures

#### 1. nice

Permite ajustar la prioridad en función de qué tan "amable"(`nice`) es un `thread`, y los valores más altos indican una voluntad de ceder tiempo de CPU.

```c
//En:  threads/thread.h

struct thread
{
  ...
  int nice;
  ...
}
```

#### 2. recent_cpu

El atributo `recent_cpu` representa el uso reciente de la CPU de un `thread`.

```c
//En:  threads/thread.h

struct thread
{
  ...
  int recent_cpu;
  ...
}
```

#### 3. load_avg

La variable global `load_avg` representa el promedio de carga del sistema, lo que indica el número promedio de `thread` listos para ejecutarse.

```c
//En:  threads/thread.c

int load_avg;
```

### Algorithms

#### 1. bsd_priority

El 4.4BSD scheduler usa MLFQS, el cual implementa un nuevo método para calcular la prioridad.

```c
// Fórmula para calcular la prioridad
priority = PRI_MAX - (recent_cpu / 4) - (nice * 2)
```

```c
// En: threads/thread.c

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
```

#### 2. bsd_recent_cpu

Función que se encarga de calcular el `recent_cpu` visto en la funcion `bsd_priority`.

```c
// Fórmula para calcular el recent_cpu
recent_cpu = decay * recent_cpu + nice

decay =  (2 * load_avg)/(2 * load_avg + 1)
```

```c
//En: threads/thread.c

void bsd_recent_cpu(struct thread *t){
  int load_avg_2 = multiply_fixed_point_and_int(load_avg, 2);
  int decay = divide_fixed_points(load_avg_2, add_fixed_point_and_int(load_avg_2, 1));
  t->recent_cpu = add_fixed_point_and_int(multiply_fixed_points(decay, t->recent_cpu), t->nice);
}
```

#### 3. load_avg

Funcion que se encargar de calcular el `load_avg` visto en la funcion `bsd_recent_cpu`.

```c
// Fórmula para calcular el load_avg
load_avg = (59/60) * load_avg + (1/60) * ready_threads
```

```c
En: threads/thread.c

void bsd_load_avg(void){
  int ready_threads = list_size(&ready_list);
  if(thread_current() != idle_thread){
    ready_threads++;
  }
  load_avg = divide_fixed_point_and_int(add_fixed_points(multiply_fixed_point_and_int(load_avg,59),int_to_fixed_point(ready_threads)),60);
}
```

#### 4. Operaciones para usar floats

En el kernel de PintOS, solo se puede realizar operaciones entre enteros. Asi que debemos implementar operaciones para usar numeros reales usando aritmetica enteros. Estas operaciones se usaron en los algoritmos anteriores.

Se implementó la representación numérica de punto fijo 17.14, porque hay 17 bits antes del punto decimal, 14 bits después y un bit de signo.

```
n: Entero
x,y: Numeros de punto fijo
F: 1 en el formato 17.14
```

|                     Operación                    |                         Cálculo                         |
|:------------------------------------------------:|:-------------------------------------------------------:|
|            Convertir `n` a punto fijo            |                          `n * F`                        |
|     Convertir `x` a `int`(redondeado hacia 0)    |                         `n / F`                         |
| Convertir `x` a `int`(redondeado al mas cercano) |  `(x + F / 2) / F si x >= 0; (x - F / 2) / F si x <= 0` |
|                  Sumar `x` y `y`                 |                         `x + y`                         |
|                 Restar `x` y `y`                 |                         `x - y`                         |
|                  Sumar `x` y `n`                 |                      `x + (n * F)`                      |
|                 Restar `x` y `n`                 |                      `x - (n * F)`                      |
|              Multiplicar `x` por `y`             |                  `((int64_t)x) * y / F`                 |
|              Multiplicar `x` por `n`             |                         `x * n`                         |
|               Dividir `x` entre `y`              |                   ((int64_t)x) * F / y                  |
|               Dividir `x` entre `n`              |                         `x / n`                         |

##### C2: Supongamos que los `threads` A, B y C tienen los valores de `nice` 0, 1 y 2. Cada uno tiene un valor de `recent_cpu` de 0. Complete la siguiente tabla que muestra la decisión de `scheduling` y los valores de `priority` y `recent_cpu` para cada `thread` después de cada número determinado de `ticks` del timer

|               | `recent_cpu` | `priority`   |               |
|:-------------:|:------------:|:------------:|:-------------:|
| timer `ticks` |  A \| B \| C |  A \| B \| C | thread to run |
|       0       |  0 \| 1 \| 2 | 63 \|61 \|59 |       A       |
|       4       |  4 \| 1 \| 2 | 62 \|61 \|59 |       A       |
|       8       |  7 \| 2 \| 4 | 61 \|61 \|58 |       B       |
|       12      |  6 \| 6 \| 6 | 61 \|59 \|58 |       A       |
|       16      |  9 \| 6 \| 7 | 60 \|59 \|57 |       A       |
|       20      | 12 \| 6 \| 8 | 60 \|59 \|57 |       A       |
|       24      | 15 \| 6 \| 9 | 59 \|59 \|57 |       B       |
|       28      | 14 \|10 \|10 | 59 \|58 \|57 |       A       |
|       32      | 16 \|10 \|11 | 58 \|58 \|56 |       B       |
|       36      | 15 \|14 \|12 | 59 \|57 \|56 |       A       |

##### C3: ¿Alguna ambigüedad en la especificación del `scheduler` hizo que los valores de la tabla fueran inciertos?  Si es así, ¿qué regla usaste para resolverlos?  ¿Coincide esto con el comportamiento de su `scheduler`?

Sí, hubo ambigüedades en la especificación del `scheduler` que hicieron que algunos valores de la tabla fueran inciertos. Específicamente, las ambigüedades estaban relacionadas con el comportamiento exacto del `scheduler` cuando varios `threads` tienen la misma prioridad y cómo se actualizan los valores `recent_cpu` y `priority`.
Para resolver la ambigüedades, se aplicarion la siguientes reglas:

1. `Priority Scheduling`: Cuando varios `threads` tienen la misma prioridad, el `scheduler` selecciona el `thread` que ha estado esperando por más tiempo. Esto se implementa utilizando la función `list_insert_ordered` para mantener el orden de los `hilos` en la lista de `waiters` según su prioridad.
2. Actualizando `recent_cpu` y `priority`: El valor `recent_cpu` del `thread` ejecutàndose se actualiza cada tick. Cada tick del `TIMER_FREQ`, se recalculan los valores `load_avg` y `recent_cpu` para todos los `threads`. Cada 4 ticks, la prioridad de todos los `hilos` se recalcula en función de los valores actualizados `recent_cpu` y `nice`.

Estas reglas coinciden con el comportamiento del `scheduler` implementado en nuestro código. El `scheduler` garantiza que los `threads` se seleccionen en función de su prioridad y que los valores de `recent_cpu` y `priority` se actualicen periódicamente de acuerdo con las reglas especificadas.

##### C4: ¿Cómo podría afectar el rendimiento la forma en que dividió el costo de `scheduling` entre el código dentro y fuera del contexto de interrupción?

Si los cálculos de `recent_cpu`, `load_avg` y prioridad toman demasiado tiempo. El `thread` puede ocupar mas tiempo de CPU de lo debido, incrementando su `load_avg` y `recent_cpu`, reduciendo su prioridad. Esto puede afectar en la toma de decisiones del `scheduler`, asi que, si el costo de `scheduling` dentro del contexto de interrupcion aumenta, lo mas probable es que disminuya el rendimiento.

### Rationale

##### C5: Critica brevemente tu diseño, señalando las ventajas y desventajas de tus elecciones de diseño.  Si tuvieras tiempo adicional para trabajar en esta parte del proyecto, ¿cómo elegirías refinar o mejorar tu diseño?

- Ventajas:
  - El uso de una sleep list ayuda a la eficiencia. reduciendo consumo de CPU.
  - Crear un .h con las operaciones de punto flotante en vez de implementarlas directamente dentro de las funciones. Esto le da mejor orden y las funciones no se hacen tan largas.
- Desventajas:
  - En algunas partes hay duplicacion de codigo.
  - Si el numero de threads aumenta demasiado, puede haber problemas de escalabilidad y rendimiento con los calculos.
- Puntos de mejora:
  - Refactorizar el codigo, modularizar más, eliminar duplicacion.
  - Optimizar los cálculos
  - Comentar el nuevo codigo implementado.
  - Documentar.

##### C6: La tarea explica en detalle la aritmética para matemáticas de punto fijo, pero deja abierta la posibilidad de implementarla por su cuenta.  ¿Por qué decidiste implementarlo de la manera que lo hiciste?  Si creó una capa de abstracción para matemáticas de punto fijo, es decir, un tipo de datos abstractos y/o un conjunto de funciones o macros para manipular números de punto fijo, ¿por qué lo hizo?  Si no, ¿por qué no?

Tal como dice la documentacion de PintOS, `recent_cpu` y `load_avg` son numeros floats, pero el kernel no tiene implementado las operaciones de floats. Entonces, en vez de usar floats, usamos numeros de punto fijo 17.14 para representar esos valores.

Creamos un nuevo archivo arit_operations.h donde se encuentran todas las operaciones porque era mejor que hacer los cálculos directamente en las funciones. De esta manera es más ordenado y toda la implementacion de punto fijo esta en un solo archivo fuera de threads.c donde se encuentra la funcionalidad principal.
