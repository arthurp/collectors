#include "alloc.h"
#include "shadowstack.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

void allocation_stress(long int n) {
  int *p, *prev = NULL;

  SHADOW_FRAME(o, 2);
  SHADOW_VAR(o, 0, p);
  SHADOW_VAR(o, 1, prev);
  SHADOW_END(o);
  
  for(int i=0; i < n; i++) {
    p = alloc(100);
    *p = i;

    if(prev != NULL)
      assert(*prev == i-1);

    prev = p;
    alloc_printstat();
  }

  SHADOW_POP_FRAME(o);
}

void random_test() {
  char *p1, *p2, *p3, **p4;

  SHADOW_FRAME(o, 4);
  SHADOW_VAR(o, 0, p1);
  SHADOW_VAR(o, 1, p2);
  SHADOW_VAR(o, 2, p3);
  SHADOW_VAR(o, 3, p4);
  SHADOW_END(o);

  p1 = alloc(1);
  GCROOT(p1);
  p1[0] = 42;
  p2 = alloc(12);
  GCROOT(p2);
  strcpy(p2, "forty-two");

  printf("%d (%p, %p)\n", p1[0], p1, &p1);

  p3 = p2;
  GCROOT(p3);

  p1 = NULL;

  alloc_printstat();
  printf("%s (%p, %p)\n", p2, p2, &p2);
  printf("%s (%p, %p)\n", p3, p3, &p3);

  alloc_collect();

  alloc_printstat();
  printf("%s (%p, %p)\n", p2, p2, &p2);
  printf("%s (%p, %p)\n", p3, p3, &p3);

  p4 = alloc(8);
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
  SHADOW_POP_FRAME(o);
}

void recursive_test(long int n, int d) {
  if(d > 10)
    return;
  
  int *p, *prev = NULL;

  SHADOW_FRAME(o, 2);
  SHADOW_VAR(o, 0, p);
  SHADOW_VAR(o, 1, prev);
  SHADOW_END(o);
  
  for(int i=0; i < n; i++) {
    printf("%d %d\n", d, i);

    p = alloc(100);
    *p = i;

    recursive_test(n, d+1);

    if(prev != NULL)
      assert(*prev == i-1);
    assert(*p == i);

    prev = p;
  }

  SHADOW_POP_FRAME(o);
}


int main(int argc, char* argv[]) {
  alloc_init(1*1000*1000);
  alloc_printstat();

  /*allocation_stress(1000);
  alloc_printstat();

  random_test();

  alloc_printstat();*/

  recursive_test(3, 0);
}
