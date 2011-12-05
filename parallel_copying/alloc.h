#include <stdlib.h>

int alloc_init();
int alloc_collect();
void alloc_printstat();

void* alloc(unsigned long int n);

#define GCROOT(localvar) asm("/* GCROOT %0 */" : "=g"(localvar) : "0"(localvar) : "memory")
