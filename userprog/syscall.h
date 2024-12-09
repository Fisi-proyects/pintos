#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include <stdbool.h>
#define PHYS_BASE 0xc0000000
#define STACK_BOTTOM 0x8048000

typedef int pid_t;

void syscall_init (void);
bool validate_address (void * addr);
void get_argument (int *esp, int *arg, int count);

/* Aqui se declararian el resto de las 13 syscalls */
/* Pero en este caso solo nos piden la implementacion de la syscall write*/
int syscall_write (int fd, const void *buffer, unsigned size);

#endif /**< userprog/syscall.h */
