#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/filesys.h"

static void syscall_handler (struct intr_frame *);
struct lock file_lock;
void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}


/* unicamente para poder probar la syscall write se implmentara un parte del
la funcion que pasa los argumentos */

bool validate_address (void *addr)
{
  if (addr >= STACK_BOTTOM && addr < PHYS_BASE && addr != 0)
    return true;

  return false;
}

void 
get_argument (int *esp, int *arg, int count)
{
  int i;
  for (i = 0; i < count; i++)
  {

    if (!validate_address(esp + 1 + i)) { sys_exit(-1); }
    arg[i] = *(esp + 1 + i);
  }
}


static void
syscall_handler (struct intr_frame *f)
{
  if (!validate_address (f->esp))
  {
    sys_exit (-1);
  }
  
  thread_current()->esp = f->esp;
  
  int argv[3];

  switch (*(int *)f->esp)
  {
    case SYS_WRITE:
      get_argument (f->esp, &argv[0], 3);
      if (!validate_address ((void*) argv[1])) 
        sys_exit (-1);

      f->eax = syscall_write ((int) argv[0], (const void*) argv[1], (unsigned) argv[2]);
      break;
  }
}




/* pcb solo se activa en tiempo de ejecucion mediante una excepcion
cuando el usuario intenta llamar a una syscall */
int 
syscall_write (int fd, const void *buffer, unsigned size)
{
  if (!validate_address((void *)buffer)) {
    sys_exit(-1); // Dirección de buffer no válida
  }

  int fd_count = thread_current()->pcb->fd_count;
  if (fd >= fd_count || fd < 1)
  {
    sys_exit(-1); // Descriptor de archivo no válido
  } 
  else if (fd == 1) // Escribir en consola (stdout)
  {
    lock_acquire(&file_lock);
    putbuf(buffer, size);
    lock_release(&file_lock);
    return size;
  } 
  else 
  {
    struct file *file = thread_current()->pcb->fd_table[fd];
    if (file == NULL) {
      sys_exit(-1); // Archivo no encontrado
    }

    // Leer contenido antes de escribir
    char *read_buffer = malloc(size);
    if (read_buffer == NULL) {
      sys_exit(-1); // Error al asignar memoria
    }

    lock_acquire(&file_lock);
    int bytes_read = file_read(file, read_buffer, size);
    lock_release(&file_lock);

    if (bytes_read < 0) {
      free(read_buffer); // Liberar memoria antes de salir
      sys_exit(-1); // Error al leer el archivo
    }

    // Escribir después de leer
    lock_acquire(&file_lock);
    int bytes_written = file_write(file, buffer, size);
    lock_release(&file_lock);

    free(read_buffer); // Liberar memoria después de usarla
    return bytes_written;
  }
}