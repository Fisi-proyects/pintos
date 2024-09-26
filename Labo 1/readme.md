# Laboratorio 1

## Grupo

- Jeremy Rosillo Ramirez
- Sebastian Cueto Salazar: <sebastian.cueto@unmsm.edu.pe>
- Riccardo Calderon Flores: <ricardo.calderon4@unmsm.edu.pe>

---

## Alarm Clock

### Data Estructures

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
