# Laboratorio 2

## Integrantes

- Jeremy Rosillo Ramirez: <jeremy.rosillo@unmsm.edu.pe>
- Sebastian Cueto Salazar: <sebastian.cueto@unmsm.edu.pe>
- Ricardo Calderon Flores: <ricardo.calderon4@unmsm.edu.pe>

## Syscall Write

### 1. Implementación de `syscall_write`
```c
int 
syscall_write (int fd, const void *buffer, unsigned size)
{
  int fd_count = thread_current()->pcb->fd_count;

  // Verificar si el descriptor de archivo es válido
  if (fd >= fd_count || fd < 1) {
    sys_exit(-1);
  }
  
  // Manejar el caso de escritura en stdout
  else if (fd == 1) {
    lock_acquire(&file_lock);
    putbuf(buffer, size);
    lock_release(&file_lock);
    return size;
  } 
  
  // Manejar la escritura en un archivo
  else {
    struct file *file = thread_current()->pcb->fd_table[fd];

    // Verificar si el archivo asociado al descriptor existe
    if (file == NULL) {
      sys_exit(-1);
    }

    int bytes_written;

    // Bloquear acceso concurrente al archivo
    lock_acquire(&file_lock);
    bytes_written = file_write(file, buffer, size);
    lock_release(&file_lock);

    return bytes_written;
  }

  return -1; // En caso de error inesperado
}
```

### Descripción de `syscall_write`
La implementación de `syscall_write` considera tres casos principales:

1. **Descriptor de archivo inválido:**
   Si el descriptor de archivo (`fd`) no es válido, la función termina inmediatamente con un error llamando a `sys_exit(-1)`.

2. **Escritura en `stdout`:**
   Si el descriptor de archivo es `1`, lo que indica salida estándar (consola), utiliza la función `putbuf` para escribir los datos en la consola. Se utiliza un bloqueo (`file_lock`) para evitar condiciones de carrera durante el acceso concurrente.

3. **Escritura en un archivo:**
   En este caso, la función verifica que el descriptor de archivo corresponda a un archivo abierto. Si el archivo es válido, utiliza la función `file_write` para escribir en el archivo mientras asegura exclusión mutua con `file_lock`.

---

### 2. Modificación de `syscall_handler`
```c
static void
syscall_handler (struct intr_frame *f)
{
  // Validar que la dirección de la pila sea válida
  if (!validate_address(f->esp)) {
    sys_exit(-1);
  }

  thread_current()->esp = f->esp;

  int argv[3];

  switch (*(int *)f->esp) {
    case SYS_WRITE:
      // Obtener argumentos de la pila
      get_argument(f->esp, &argv[0], 3);

      // Validar que el buffer sea válido
      if (!validate_address((void *)argv[1])) {
        sys_exit(-1);
      }

      // Llamar a syscall_write y almacenar el resultado en eax
      f->eax = syscall_write((int)argv[0], (const void *)argv[1], (unsigned)argv[2]);
      break;

    // Otros casos (no modificados en esta versión)
  }
}
```

### Descripción de `syscall_handler`

1. **Validación inicial:**
   Se verifica que la dirección de la pila sea válida utilizando `validate_address`. Si no lo es, se llama a `sys_exit(-1)` para terminar el proceso.

2. **Obtención de argumentos:**
   Utiliza la función `get_argument` para extraer los argumentos necesarios de la pila (en este caso, tres argumentos).

3. **Validación del buffer:**
   Antes de proceder, verifica que la dirección del buffer sea válida llamando nuevamente a `validate_address`.

4. **Llamada a `syscall_write`:**
   Finalmente, llama a la función `syscall_write` con los argumentos extraídos y almacena el resultado en el registro `eax` del marco de interrupción.

---

### Consideraciones finales
La implementación asegura exclusión mutua utilizando bloqueos (`file_lock`) para proteger las operaciones de escritura, previniendo así condiciones de carrera. Además, la validación exhaustiva de los argumentos garantiza que las operaciones de sistema no se ejecuten con datos inválidos, lo que contribuye a la estabilidad del sistema.


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

##### 2. Carga de Página (load_page)

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

##### 3. Controlador de Fallos de Página (page_fault)


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

### ESTRUCTURA

#### 1. ESP

```c
//Archivo: threads/thread.h

struct thread{
  ...
  void *esp;
  ...
}
```
Para hacer crecer el stack dinamicamante, es necesario registrar el puntero del stack para cada `thread`. Por lo tanto, se agregó el campo `esp` para almacenar el puntero del stack.

#### 2. Limite del stack
```c
//Archivo: userprog/exception.h

#define MAX_STACK_SIZE (8 * 1024 * 1024)
```
El limite de tamaño del stack se estableció en 8MB porque es el default en muchos sistemas GNU/Linux.
### ALGORITMO
#### 1. Page Fault
```c
//Archivo: userprog/exception.c

static void
page_fault(struct intr_frame *f) {
  ...
  upage = pg_round_down(fault_addr);
  if (is_kernel_vaddr (fault_addr) || !not_present) {
    sys_exit (-1);
  }
   
  spt = &thread_current()->spt;
  spe = get_spte(spt, upage);

  esp = user ? f->esp : thread_current()->esp;
  if (fault_addr >= esp - 32 && fault_addr < PHYS_BASE && fault_addr >= PHYS_BASE - MAX_STACK_SIZE) {
    if (!get_spte(spt, upage)) {
      init_zero_spte (spt, upage);
    }
  }
  ...
}
```
Esta función se llama cuando ocurre un page fault. Determina la causa del fallo y toma las medidas adecuadas para manejarlo. Si la dirección que
causó el fallo es una dirección del kernel o la página ya está presente, el proceso se termina. De lo contrario, intenta cargar la página desde la 
tabla de páginas suplementaria (`spt`) y, si es necesario, aumenta el tamaño del stack. Cuando ocurre un page fault, si la dirección donde esto ocurrió y el `esp` del `thread` actual están significativamente cerca por una diferencia de `32`, se crea una entrada de tabla de páginas suplementarias(`spt`) llena con `0` y se asigna como espacio libre en el stack.

#### 2. Setup Stack
```c
//Archivo: userprog/process.c

static bool
setup_stack(void **esp) {
  uint8_t *kpage;
  bool success = false;

  kpage = falloc_get_page(PAL_USER | PAL_ZERO, PHYS_BASE - PGSIZE);
  if (kpage != NULL) {
    success = install_page(((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);

    if (success) {
      init_frame_spte(&thread_current()->spt, PHYS_BASE - PGSIZE, kpage);
      *esp = PHYS_BASE;
    } else {
      falloc_free_page(kpage);
    }      
  }
  
  return success;
}
```
Esta función intenta asignar una página de memoria usando `falloc_get_page` con las flags `PAL_USER | PAL_ZERO` y la dirección `PHYS_BASE - PGSIZE`. Si la asignación es exitosa, instala la página en la tabla de páginas con `install_page`. Si la instalación es exitosa, inicializa la entrada de la tabla de páginas suplementaria del frame con `init_frame_spte` y establece el puntero del stack `esp` a `PHYS_BASE`. Si la instalación falla, libera la página asignada.

#### A1: Explique su heurística para decidir si un error de página para una dirección virtual no válida debería hacer que el stack se extienda a la página que falló.
1. **Dirección de fallo dentro del rango del stack**: La dirección que causó el fallo (`fault_addr`) debe estar dentro del rango permitido para el stack. Esto se verifica con la condición:
```c
if (fault_addr >= esp - 32 && fault_addr < PHYS_BASE && fault_addr >= PHYS_BASE - MAX_STACK_SIZE)
```
Aquí, `esp` es el puntero al stack actual, `PHYS_BASE` es la dirección base de la memoria física y `MAX_STACK_SIZE` es el tamaño máximo permitido para el stack (8 MB en este caso).

2. **Página no presente**: La página que causó el fallo no debe estar presente en la memoria. Esto se verifica con la condición:
```c
if (!not_present)
```
Aquí, `not_present` es una variable booleana que indica si la página no está presente.

3. **Inicialización de una nueva entrada de página cero**: Si la dirección de fallo está dentro del rango del stack y la página no está presente, se inicializa una nueva entrada de página cero en la tabla de páginas suplementaria (SPT) con:
```c
if (!get_spte(spt, upage)) {
  init_zero_spte(spt, upage);
}
```