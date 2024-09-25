# Laboratorio 1
## GROUP

- Jeremy Rosillo Ramirez
- Sebastian Cueto Salazar
- Riccardo Calderon Flores ricardo.calderon4@unmsm.edu.pe


# Alarm Clock
## Data Estructures

### A1 
~~~
struct thread {
  tid_t tid;  // Identificador del hilo.
  int64_t remain_time_to_wake_up;  // Tiempo restante para que el hilo se despierte.
  enum thread_status status;  // Estado del hilo.
  struct list_elem elem;  // Elemento de lista para listas de hilos.
};
~~~ 
- `remain_time_to_wake_up`: Almacena el tiempo (en ticks) cuando el hilo debe ser despertado.
- `elem`: Permite que el hilo sea parte de la lista de hilos dormidos o listos.

## Algorithms
### A2
El proceso de poner un hilo a dormir usando `timer_sleep()` implica bloquear el hilo y añadirlo a una lista de hilos dormidos ordenada. En cada interrupción de timer, se verifica si algún hilo ha completado su tiempo de sueño para desbloquearlo.
- Algoritmo de `timer_sleep()`:

~~~
void timer_sleep(int64_t ticks) {
    if (ticks <= 0) {
        return;
    }
    set_thread_sleep(ticks);  // Pone el hilo a dormir
}
~~~
- Algoritmo de `set_thread_sleep()`:

La función `set_thread_sleep()` desactiva las interrupciones, ajusta el tiempo de despertar del hilo y lo inserta en la lista de hilos dormidos en el lugar correcto. Luego, bloquea el hilo hasta que se despierte.
~~~
void set_thread_sleep(int64_t ticks) {
    struct thread *cur = thread_current();
    enum intr_level old_level;

    ASSERT (!intr_context());
    ASSERT (cur != idle_thread);

    old_level = intr_disable();  // Deshabilitar interrupciones
    cur->remain_time_to_wake_up = timer_ticks() + ticks;  // Establece el tiempo de despertar
    list_insert_ordered(&sleep_thread_list, &cur->elem, (list_less_func *) &compare_ticks, NULL);  // Inserta en la lista de hilos dormidos
    thread_block();  // Bloquea el hilo
    intr_set_level(old_level);  // Restaura el nivel de interrupción
}
~~~
- Algoritmo de `wake_up_thread()`:

`wake_up_thread()` se ejecuta en cada interrupción de temporizador y revisa si algún hilo ha terminado su periodo de sueño. Si es así, se desbloquea y se elimina de la lista.
~~~
void wake_up_thread(int64_t current_ticks) {
    struct list_elem *e = list_begin(&sleep_thread_list);
    while (e != list_end(&sleep_thread_list)) {
        struct thread *t = list_entry(e, struct thread, elem);
        if (t->remain_time_to_wake_up > current_ticks) {
            break;  // Si aún no es el momento de despertar
        }
        e = list_remove(e);  // Elimina de la lista de hilos dormidos
        thread_unblock(t);  // Desbloquea el hilo
    }
}
~~~
- Función auxiliar `compare_ticks()`:

compare_ticks() es una función auxiliar que compara los tiempos de despertar de dos hilos para mantener la lista ordenada.
~~~
bool compare_ticks(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED) {
    struct thread *t_a = list_entry(a, struct thread, elem);
    struct thread *t_b = list_entry(b, struct thread, elem);
    return t_a->remain_time_to_wake_up < t_b->remain_time_to_wake_up;
}
~~~
## Synchronization
### A4:
Para evitar condiciones de carrera cuando múltiples hilos llaman a `timer_sleep()` al mismo tiempo, se desactivan las interrupciones durante la inserción de los hilos en la lista de dormidos y al procesar la lista durante las interrupciones del temporizador. Esto asegura que no haya conflictos mientras se actualizan las estructuras de los hilos.

### A5:
Si ocurre una interrupción de temporizador mientras se está ejecutando `timer_sleep()`, las interrupciones ya estarán deshabilitadas por `intr_disable()`. Esto evita que haya interferencia mientras se manipula la lista de hilos dormidos.

## Rationale

### A6:
Este diseño fue elegido porque optimiza la eficiencia en el manejo de los hilos dormidos. Al usar una lista ordenada, el tiempo que se invierte en procesar los hilos dormidos en cada tick se minimiza. Comparado con un enfoque no ordenado o con búsquedas lineales, este método reduce la carga de procesamiento y hace que el temporizador sea más eficiente.
