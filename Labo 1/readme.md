# Laboratorio 1

## Grupo


- Jeremy Rosillo Ramirez: <jeremy.rosillo@unmsm.edu.pe>
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


##### C2: Supongamos que los `threads` A, B y C tienen los valores de `nice` 0, 1 y 2. Cada uno tiene un valor de `recent_cpu` de 0. Complete la siguiente tabla que muestra la decisión de `scheduling` y los valores de `priority` y `recent_cpu` para cada `thread` después de cada número determinado de `ticks` del timer:


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

