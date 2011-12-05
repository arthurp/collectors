#include <stdlib.h>

int alloc_init();
int alloc_collect();
void alloc_printstat();

void* alloc(unsigned long int n);

void alloc_safe_point();

#define GCROOT(localvar) asm("/* GCROOT %0 */" : "=g"(localvar) : "0"(localvar) : "memory")
