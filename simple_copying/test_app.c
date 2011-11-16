#include "alloc.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char* argv[]) {
  alloc_init(1*1000*1000);
  alloc_printstat();

  char* p1 = alloc(1);
  GCROOT(p1);
  p1[0] = 42;
  char* p2 = alloc(12);
  GCROOT(p2);
  strcpy(p2, "forty-two");

  printf("%d (%p, %p)\n", p1[0], p1, &p1);

  char* p3 = p2;
  GCROOT(p3);

  p1 = NULL;

  alloc_printstat();
  printf("%s (%p, %p)\n", p2, p2, &p2);
  printf("%s (%p, %p)\n", p3, p3, &p3);

  alloc_collect();

  alloc_printstat();
  printf("%s (%p, %p)\n", p2, p2, &p2);
  printf("%s (%p, %p)\n", p3, p3, &p3);

  char** p4 = alloc(8);
  GCROOT(p4);
  *p4 = p2;
  p2 = alloc(15);
  strcpy(p2, "forty-three");

  p3 = p2;

  p1 = NULL;

  alloc_printstat();
  printf("%s (%p, %p)\n", p2, p2, &p2);
  printf("%s (%p, %p)\n", p3, p3, &p3);
  printf("%s (%p, %p)\n", *p4, *p4, &p4);

  alloc_collect();

  alloc_printstat();
  printf("%s (%p, %p)\n", p2, p2, &p2);
  printf("%s (%p, %p)\n", p3, p3, &p3);
  printf("%s (%p, %p)\n", *p4, *p4, &p4);


  alloc_collect();

  alloc_printstat();
  printf("%s (%p, %p)\n", p2, p2, &p2);
  printf("%s (%p, %p)\n", p3, p3, &p3);
  printf("%s (%p, %p)\n", *p4, *p4, &p4);

  p2 = p3 = NULL;
  *p4 = NULL;

  alloc_collect();

  alloc_printstat();

  p4 = NULL;

  alloc_collect();

  alloc_printstat();
}
