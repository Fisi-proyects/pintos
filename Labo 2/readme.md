# Laboratorio 2

## Integrantes

- Jeremy Rosillo Ramirez: <jeremy.rosillo@unmsm.edu.pe>
- Sebastian Cueto Salazar: <sebastian.cueto@unmsm.edu.pe>
- Ricardo Calderon Flores: <ricardo.calderon4@unmsm.edu.pe>

## Syscall Write

### 1. Process Context Block

```c
struct pcb
  {
    int exit_code;
    bool is_exited;
    bool is_loaded;

    struct file **fd_table;
    int fd_count;
    struct file *file_ex;

    struct semaphore sema_wait;
    struct semaphore sema_load;
  };
```

```c
struct thread
  {
    // ...
#ifdef USERPROG
    // ...
    struct pcb *pcb;                    /* PCB. */
    // ...
  };
```

Para gestionar la información de cada proceso, se diseñó e implementó la estructura `pcb`, que se integra en cada `thread` para mantener su contexto. Dentro de esta estructura, el campo `exit_code` almacena el código de salida definido por la llamada al sistema `exit`, mientras que los indicadores `is_exited` e `is_loaded` reflejan el estado actual del proceso.

Los descriptores de archivo asignados al proceso se controlan mediante `fd_table` y `fd_count`, y el archivo ejecutado por el proceso es referenciado a través de `file_ex`. Para garantizar la sincronización entre los procesos, se incorporaron semáforos en la PCB que permiten la gestión de operaciones de `wait` y `load`, los cuales son inicializados al momento de la creación del hilo en `thread_create`.

### 2. Relación padre-hijo

```c
struct thread
  {
    // ...
#ifdef USERPROG
    // ...
    struct thread *parent_process;
    struct list list_child_process;
    struct list_elem elem_child_process;
#endif
    // ...
  };
```

El campo anterior se agregó a la estructura del hilo para gestionar y hacer seguimiento de la información tanto del proceso principal como de los procesos secundarios.

Cuando un hilo ejecuta la función `exec`, se genera un nuevo proceso y la estructura descrita se utiliza para representar estos procesos en una jerarquía de tipo árbol, con una relación padre-hijo. El proceso que ejecutó la creación del nuevo proceso se guarda en el campo `parent_process`, mientras que los procesos secundarios generados se almacenan en el campo `list_child_process`.

Estos campos son inicializados durante la creación del hilo, específicamente en la función `thread_create`.

### ALGORITMO

#### 1. Inicialización de la nueva estructura de datos

```c
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{
  struct thread *t;
  // ...
  t->parent_process = thread_current ();

  t->pcb = palloc_get_page (0);

  if (t->pcb == NULL) {
    return TID_ERROR;
  }

  t->pcb->fd_table = palloc_get_page (PAL_ZERO);

  if (t->pcb->fd_table == NULL) {
    palloc_free_page (t->pcb);
    return TID_ERROR;
  }

  t->pcb->fd_count = 2;
  t->pcb->file_ex = NULL;
  t->pcb->exit_code = -1;
  t->pcb->is_exited = false;
  t->pcb->is_loaded = false;

  sema_init (&(t->pcb->sema_wait), 0);
  sema_init (&(t->pcb->sema_load), 0);

  list_push_back (&(t->parent_process->list_child_process), &(t->elem_child_process));
  // ...
}
```

Para asegurar que el process context se mantenga correctamente, los campos añadidos deben ser inicializados cuando se crea el proceso. Por eso, se implementó que, al ejecutar `thread_create()`, estos campos se inicialicen adecuadamente.

#### 2. Execute Process

```c
tid_t
process_execute (const char *file_name) 
{
  char *fn_copy, *parsed_fn;
  tid_t tid;

  // ...

  parsed_fn = palloc_get_page (0);
  if (parsed_fn == NULL) {
    return TID_ERROR;
  }

  strlcpy (parsed_fn, file_name, PGSIZE);

  pars_filename (parsed_fn);

  tid = thread_create (parsed_fn, PRI_DEFAULT, start_process, fn_copy);
  if (tid == TID_ERROR) {
    palloc_free_page (fn_copy); 
  } else {
    sema_down (&(get_child_pcb (tid)->sema_load));
  }
  
  palloc_free_page (parsed_fn);

  return tid;
}
```

Después de llamar a la función `pars_filename()` en el comando recibido para analizar el nombre del archivo, se utiliza esta información para crear un hilo con el archivo ejecutable correcto. Durante este proceso, se emplea el semáforo `load_sema` para evitar que el proceso padre termine antes de que el hijo haya cargado, bloqueando al padre hasta que el hijo haya terminado de cargar.

##### 2.1. Load Sync.

```c
static void
start_process (void *file_name_)
{
  // ...

  sema_up (&(thread_current ()->pcb->sema_load));

  // ...
}
```

En `process_execute()`, el semáforo `sema_load`, que está **DOWN** en el proceso padre, se pone **UP** en `start_process()` una vez que el proceso hijo ha terminado de cargar. Esto permite que el proceso padre espere a que el proceso hijo termine de cargar antes de continuar.

```c
void 
sys_exit (int status)
{
  struct thread *t = thread_current ();
  t->pcb->exit_code = status;
  if (!t->pcb->is_loaded)
    sema_up (&(t->pcb->sema_load));

  printf ("%s: exit(%d)\n", t->name, status);
  thread_exit ();
}
```

Si un proceso termina antes de que el semáforo `sema_load` se ponga **UP** durante la carga, podría ocurrir una situación en la que el proceso padre se quede esperando indefinidamente. Para evitar esto, se implementó que, en `sys_exit()`, si el proceso termina mientras está cargando, el semáforo `sema_load` se ponga **UP**. Esto permite que el proceso padre salga de la espera infinita.

#### 3. Waiting Process

```c
int
process_wait (tid_t child_tid) 
{
  struct thread *child = get_child_thread (child_tid);
  int exit_code;

  if (child == NULL)
    return -1;
  
  if (child->pcb == NULL || child->pcb->exit_code == -2 || !child->pcb->is_loaded) {
    return -1;
  }
  
  sema_down (&(child->pcb->sema_wait));
  exit_code = child->pcb->exit_code;

  list_remove (&(child->elem_child_process));
  palloc_free_page (child->pcb);
  palloc_free_page (child);

  return exit_code;
}
```

Cuando se recibe el `child_tid` para el proceso que se va a esperar, el proceso padre pone el semáforo `sema_wait` del proceso hijo en **DOWN** para que el padre espere hasta que el proceso hijo termine. El hecho de que el proceso padre esté esperando al hijo significa que el proceso hijo ha terminado completamente, por lo que, durante el proceso de espera, se liberan los recursos del hijo para evitar fugas de memoria.

##### 3.1. Unblocking Waiting Process

```c
void
process_exit (void)
{
  // ...
  cur->pcb->is_exited = true;
  sema_up (&(cur->pcb->sema_wait));
  // ...
}
```

Cuando el proceso hijo termina, es decir, durante el proceso `process_exit()`, se pone en **UP** su propio semáforo `sema_wait`. Esto permite que el proceso padre, que está esperando a este proceso hijo, se desbloquee y continúe su ejecución.

#### 4. Exiting Process

```c
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;
  int i;

  for (i = cur->pcb->fd_count - 1; i > 1; i--)
  {
    sys_close (i);
  }

  palloc_free_page (cur->pcb->fd_table);

  // ...
}
```

Cuando un proceso termina, se deben cerrar todos los archivos correspondientes a los descriptores de archivo que el proceso estaba gestionando. Posteriormente, para evitar fugas de memoria, se libera la memoria asignada a la `fd_table` del proceso.

### Discusión

#### 1. Sincronización

Para garantizar la sincronización entre los procesos padre e hijo, se introdujeron los semáforos `sema_wait` y `sema_load`. Además, se agregaron los indicadores de estado `is_loaded` e `is_exited` para hacer un seguimiento del estado de los procesos. Esto permitió detectar situaciones como la terminación del proceso con un lock aún en posesión o la finalización anómala de un proceso hijo, y responder adecuadamente a estas condiciones.

#### 2. Gestión de Memoria

Para evitar fugas de memoria, se liberó toda la memoria asignada cuando un proceso terminaba. Después de la asignación de memoria, se verificó si la asignación fue exitosa. En caso de que se produjera un desbordamiento de memoria, el proceso se terminaba para prevenir fallos de página debido a accesos a memoria inválida.

## Lazy Loading

*Para el desarrollo del lazy loading no es necesario como tal la implementacion de un `Frame Table`  pero lo implementamos de igual manera para facilitarnos ciertas cosas necesarias dentro del lazy loading como la interaccion con el `page_handler`*

### Tabla de Marcos (Frame Table)

#### Estructuras de Datos

##### 1. Entrada de la Tabla de Marcos

```c
struct fte {
    void *kpage;
    void *upage;

    struct thread *t;

    struct list_elem list_elem;
};
```

La estructura `fte` representa cada entrada en la tabla de marcos y está diseñada para considerar cada marco como una unidad. `kpage` almacena la página virtual del kernel y `upage` almacena la página virtual del usuario. `t` apunta al hilo propietario de la entrada, y la entrada se gestiona en la lista `frame_table` mediante `list_elem`.

##### 2. Tabla de Marcos

```c
static struct list frame_table;
```

frame_table es una lista que contiene las entradas `fte` y constituye la estructura principal de la tabla de marcos.

##### 3. Bloqueo para la Tabla de Marcos

```c
static struct lock frame_lock;
```

Para evitar problemas de sincronización al acceder simultáneamente a la tabla de marcos desde múltiples procesos, se utiliza un bloqueo (frame_lock) para proteger las secciones críticas.

##### 4. clock_cursor

```c
static struct fte *clock_cursor;
```

Cuando no hay marcos libres, este campo ayuda a encontrar un marco para la expulsión.

#### Algoritmos

##### 1. frame_init ()

```c
void
frame_init() {
    list_init(&frame_table);
    lock_init(&frame_lock);
    clock_cursor = NULL;
}
```

```c
int
main (void) {
    // ...
    frame_init();
    // ...
}
```

Se inicializan las estructuras de datos necesarias para gestionar la tabla de marcos. frame_table se inicializa con `list_init()`, el bloqueo se inicializa con `lock_init()`, y clock_cursor se establece en NULL. Esta función se llama en el proceso de inicialización del sistema de hilos (main ()).

#### 2. falloc_get_page ()

```c
void *
falloc_get_page(enum palloc_flags flags, void *upage) {
    struct fte *e;
    void *kpage;
    lock_acquire(&frame_lock);
    kpage = palloc_get_page(flags);
    if (kpage == NULL) {
        evict_page();
        kpage = palloc_get_page(flags);
        if (kpage == NULL)
            return NULL;
    }

    e = (struct fte *) malloc(sizeof *e);
    e->kpage = kpage;
    e->upage = upage;
    e->t = thread_current();
    list_push_back(&frame_table, &e->list_elem);

    lock_release(&frame_lock);
    return kpage;
}
```

Esta función asigna un nuevo marco como una entrada fte. Utiliza `palloc_get_page()` para asignar kpage correspondiente a upage. Si no hay marcos disponibles, se llama a `evict_page()` para liberar espacio. La entrada recién creada se añade a frame_table. El acceso a `frame_table` está protegido por `frame_lock`.

#### 3. falloc_free_page ()

```c
void
falloc_free_page(void *kpage) {
    struct fte *e;
    lock_acquire(&frame_lock);
    e = get_fte(kpage);
    if (e == NULL)
        sys_exit(-1);

    list_remove(&e->list_elem);
    palloc_free_page(e->kpage);
    pagedir_clear_page(e->t->pagedir, e->upage);
    free(e);

    lock_release(&frame_lock);
}
```

Libera un marco previamente asignado. Recibe kpage como parámetro, encuentra la entrada correspondiente en `frame_table` y la elimina.

#### 4. get_fte ()

```c
struct fte *
get_fte(void *kpage) {
    struct list_elem *e;
    for (e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(e))
        if (list_entry(e, struct fte, list_elem)->kpage == kpage)
    return list_entry(e, struct fte, list_elem);
    return NULL;
}
```

Esta función busca y devuelve la entrada `fte` correspondiente a una página del kernel específica (kpage) recorriendo `frame_table`.

### Lazy Loading (Implementacion)

#### Algoritmos

##### 1. Carga de Segmento (load_segment)

```c
static bool
load_segment(struct file *file, off_t ofs, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
    // ...
    init_file_spte(&thread_current()->spt, upage, file, ofs, page_read_bytes, page_zero_bytes, writable);
    // ...
}
```

Se modificó el proceso para eliminar la carga inmediata de segmentos en memoria. En su lugar, se crean entradas en la tabla de páginas suplementaria (SPT) para que las páginas se carguen de manera diferida cuando se produzca un fallo de página (PF).

##### . Carga de Página (load_page)

```c
bool
load_page(struct hash *spt, void *upage) {
    struct spte *e;
    uint32_t *pagedir;
    void *kpage;
    e = get_spte(spt, upage);
    if (e == NULL)
        sys_exit(-1);
    kpage = falloc_get_page(PAL_USER, upage);
    if (kpage == NULL)
        sys_exit(-1);
    bool was_holding_lock = lock_held_by_current_thread(&file_lock);
    switch (e->status) {
        case PAGE_ZERO:
            memset(kpage, 0, PGSIZE);
            break;
        case PAGE_SWAP:
            swap_in(e, kpage);
            break;
        case PAGE_FILE:
            if (!was_holding_lock)
                lock_acquire(&file_lock);
            if (file_read_at(e->file, kpage, e->read_bytes, e->ofs) != e->read_bytes) {
                falloc_free_page(kpage);
                lock_release(&file_lock);
                sys_exit(-1);}
            memset(kpage + e->read_bytes, 0, e->zero_bytes);
            if (!was_holding_lock)
                lock_release(&file_lock);
            break;
        default:
            sys_exit(-1);
    }
    pagedir = thread_current()->pagedir;
    if (!pagedir_set_page(pagedir, upage, kpage, e->writable)) {
        falloc_free_page(kpage);
        sys_exit(-1);
    }
    e->kpage = kpage;
    e->status = PAGE_FRAME;
    return true;
}
```

Este método se llama desde el controlador de fallos de página y realiza la carga diferida de páginas. Según el estado de la entrada (`PAGE_ZERO`, `PAGE_SWAP`, o `PAGE_FILE`), la página se inicializa, se recupera del espacio de intercambio, o se carga desde un archivo. Luego, se actualiza el directorio de páginas y el estado de la entrada.

#### 3. Controlador de Fallos de Página (page_fault)

```c
static void page_fault(struct intr_frame *f) {
    // ...
    if (load_page(spt, upage)) {
        return;
    }
    // ...
}
```

Se utiliza para implementar la carga diferida aprovechando los fallos de página. Cuando se intenta acceder a una página aún no cargada en memoria, el controlador llama a `load_page()` para realizar la carga.


## Stack Grow

#### ESTRUCTURA

#### ALGORITMO
