#include "alloc.h"
#include "shadowstack.h"

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

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

static volatile int run = 1;

void * thread(void *e) {
  alloc_init(10*1000);
  pthread_yield();
  //while(run) {
  allocation_stress(300);    
    //}

  alloc_safe_point();
  sleep(1);
  alloc_safe_point();

  return NULL;
}

void run_threads(int nthreads) {
  pthread_t threads[nthreads];
  int rc;
  long t;

  for(t=0;t<nthreads;t++) {
    printf("In main: creating thread %ld\n", t);
    rc = pthread_create(&threads[t], NULL, thread, NULL);
    if (rc){
      printf("ERROR; return code from pthread_create() is %d\n", rc);
      exit(-1);
    }
  }

  sleep(2);

  run = 0;

  for(t=0;t<nthreads;t++) {
    pthread_join(threads[t], NULL);
  }
}


int main(int argc, char* argv[]) {
  //alloc_init(1*1000*1000);
  //alloc_printstat();

  /*allocation_stress(1000);
  alloc_printstat();

  random_test();

  alloc_printstat();*/

  //recursive_test(3, 0);
  
  //random_test();
  
  run_threads(atoi(argv[1]));
}
