#include "alloc.h"
#include "shadowstack.h"
#include "randomize.h"

#include <gc/gc.h>
#include <stdio.h>
#include <stdint.h>
#include <malloc.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>

#include <pthread.h>

const int alloc_align = 16;

#define TAG_SIZE 8 // 1 word

#define TAG_FROM_SIZE(size) ((size) << 16)
#define SIZE_FROM_TAG(tag) ((tag) >> 16)

#define TAG_OF_PTR(p) (*TAGPTR_OF_PTR(p))
#define TAGPTR_OF_PTR(p) ((uintptr_t*)((uintptr_t)(p) & ~(alloc_align-1)) - 1)

#define FORWARDING_PTR_MASK 0x1
#define IS_FORWARDING_PTR(t) (((t) & FORWARDING_PTR_MASK) == FORWARDING_PTR_MASK)
#define FORWARDING_PTR_OF_TAG(t) (void*)((t) & ~FORWARDING_PTR_MASK) 
#define FORWARDING_PTR_OF_PTR(p) ((uintptr_t)(p) | FORWARDING_PTR_MASK)

#define SIZE_OF_PTR(p) SIZE_FROM_TAG(TAG_OF_PTR(p))

#define ROUNDUP_PTR(p) (void*)(((uintptr_t)p + alloc_align) & ~(alloc_align-1))


#define MAX_THREADS 32


#define STATE_MUTATING 0 // Normal mutator computation
#define STATE_WAITING_FOR_COLLECTION 1 // Waiting all threads to stop and collect (Only the master goes into this state)
#define STATE_WAITING_FOR_HEAPS 2 // Waiting for the master to divide the heap up (only slaves)
#define STATE_SCANNING 4 // Actual scanning and copying this is parallel work.
#define STATE_DONE 5 // Done copying, once all threads are in this state they will all go back to MUTATING

typedef struct {
  void* alloc_ptr;
  void* alloc_end;

  void* stack_base;

  volatile int state;
} local_heap_t;

__thread local_heap_t local_heap = {0,};

#define STATE_NOT_COLLECTING 0
#define STATE_COLLECTION_NEEDED 1
#define STATE_HEAPS_SET 2

typedef struct { 
  volatile int collection_needed;

  /* Protected by init_lock */
  void* currentspace;
  void* currentspace_end;
  
  void* nextspace;
  void* nextspace_end;

  local_heap_t *thread_heap[MAX_THREADS];
} global_heap_t;

global_heap_t heap = {0,NULL,};
pthread_mutex_t init_lock = PTHREAD_MUTEX_INITIALIZER;

void alloc_init_global(size_t heap_size) {
  if( heap.currentspace != NULL ) 
    return;

  // Allocate our heap.
  int pagesize = getpagesize();
  heap_size = heap_size + (pagesize - heap_size % pagesize);
  heap.currentspace = memalign(pagesize, heap_size*2);
  heap.currentspace_end = heap.currentspace + heap_size;

  heap.nextspace = heap.currentspace + heap_size;
  heap.nextspace_end = heap.nextspace + heap_size;

  printf("Current space is  %p with size 0x%lx\n", heap.currentspace, heap_size);
  printf("Next space is  %p with size 0x%lx\n", heap.nextspace, heap_size);

  for(int i=0; i<MAX_THREADS; i++) {
    heap.thread_heap[i] = NULL;
  }

  heap.collection_needed = 0;
}

int alloc_init(size_t heap_size) {
  pthread_mutex_lock(&init_lock);
  alloc_init_global(heap_size);

  __rand((int)(uintptr_t)&heap_size);

  // Find the stack.
  struct GC_stack_base stb;
  int ret = GC_get_stack_base(&stb);
  local_heap.stack_base = stb.mem_base;
  printf("Stack base is %p (current frame is around %p)\n", local_heap.stack_base, &ret);

  for(int i=0; i < MAX_THREADS; i++) {
    if( heap.thread_heap[i] == NULL ) {
      heap.thread_heap[i] = &local_heap;
      break;
    }
  }

  pthread_mutex_unlock(&init_lock);

  alloc_collect(); // This will allocate us a peice of the heap.

  return ret;
}

inline static void* scanptr(void** p, void* next_alloc_pointer) {
  // XXX: Check for thread safety.

  void* candidate = *p;
  // Check to see if the pointer is to something in the old space.
  if(candidate >= heap.nextspace && candidate < heap.nextspace_end) {
    printf("Found pointer: %p at %p\n", candidate, p);
    
    uintptr_t tag = TAG_OF_PTR(candidate);
      
    // Check if it has already been moved.
    if(IS_FORWARDING_PTR(tag)) {
      void* newptr = FORWARDING_PTR_OF_TAG(tag);
      printf("Found pointer: %p at %p redirecting to %p\n", candidate, p, newptr);
      *p = newptr;
    } else {
      // allocate space in new space
      // round up to next alloc_align that is at least TAG_SIZE greater than next_alloc_pointer
      // then subtract TAG_SIZE
      void* newptrtag = ROUNDUP_PTR(next_alloc_pointer + TAG_SIZE) - TAG_SIZE;
      bzero(next_alloc_pointer, newptrtag - next_alloc_pointer);
      memcpy(newptrtag, TAGPTR_OF_PTR(candidate), SIZE_FROM_TAG(tag) + TAG_SIZE);
      void* newptr = newptrtag + TAG_SIZE;
      printf("Found pointer: %p at %p copying and redirecting to %p\n", candidate, p, newptr);
      *p = newptr;

      // XXX: We need to use CAS and stuff

      assert(!IS_FORWARDING_PTR(TAG_OF_PTR(newptr)));
      
      next_alloc_pointer = newptr + SIZE_FROM_TAG(tag);
      
      TAG_OF_PTR(candidate) = FORWARDING_PTR_OF_PTR(newptr); 
    }
  }

  return next_alloc_pointer;
}

int alloc_collect() {
  int pagesize = getpagesize();
  int master = 0;

  // Start waiting for other threads
  if( __sync_bool_compare_and_swap(&heap.collection_needed, 
				   STATE_NOT_COLLECTING, 
				   STATE_COLLECTION_NEEDED) ) {
    // We are the master
    master = 1;

    // swap spaces
    void* tmp;
    tmp = heap.nextspace;
    heap.nextspace = heap.currentspace;
    heap.currentspace = tmp;

    tmp = heap.nextspace_end;
    heap.nextspace_end = heap.currentspace_end;
    heap.currentspace_end = tmp;


    // Wait for every other thread to be waiting for heaps.
    local_heap.state = STATE_WAITING_FOR_COLLECTION;
    printf("%p: Waiting for collection.\n", &local_heap);
    int n_threads = 1;
    for(int i=0; i < MAX_THREADS; i++) {
      local_heap_t* t = heap.thread_heap[i];
      if( t != NULL && t != &local_heap ) {
	n_threads++;
	while(t->state != STATE_WAITING_FOR_HEAPS);
      }
    }

    // Divide up the heap
    printf("%p: Dividing heap.\n", &local_heap);
    size_t heap_size = heap.currentspace_end - heap.currentspace;
    // XXX: Assumes pagesize is a power of 2.
    size_t thread_heap_size = (heap_size/n_threads) & ~(pagesize-1);

    assert( thread_heap_size != 0 );

    void* next_alloc_pointer = heap.nextspace;
    for(int i=0; i < n_threads; i++) {
      local_heap_t* t = heap.thread_heap[i];
      t->alloc_ptr = next_alloc_pointer;
      t->alloc_end = next_alloc_pointer+thread_heap_size;
      printf("Giving %p - %p to thread %d (%p)\n", t->alloc_ptr, t->alloc_end, i, t);
      next_alloc_pointer += thread_heap_size;
    }
    assert(next_alloc_pointer <= heap.nextspace_end);

    __sync_synchronize();

    // Now we can scan
    printf("%p: Scanning\n", &local_heap);
    heap.collection_needed = STATE_HEAPS_SET;
    local_heap.state = STATE_SCANNING;    
  } else {
    // Wait for master to set heaps.
    local_heap.state = STATE_WAITING_FOR_HEAPS;
    __sync_synchronize();
    printf("%p: Waiting for heap\n", &local_heap);
    while(heap.collection_needed != STATE_HEAPS_SET);
    // Now we can scan
    printf("%p: Scanning\n", &local_heap);
    local_heap.state = STATE_SCANNING;
  }
  

  void* alloc_pointer = local_heap.alloc_ptr;
   
  {
    void* ptrQueue[8] = {NULL,};
    int ptrQueueIdx = 0;

    // Scan Shadow stack
    for(shadow_stack_frame* frame = shadow_stack_top; frame != NULL; frame = frame->prev) {
      for(int i = 0; i < frame->length; i++) {
	void* p = frame->elements[i];
	ptrQueue[ptrQueueIdx] = p;
	ptrQueueIdx++;

	if( ptrQueueIdx >= 8 ) {
	  random_iter(i, ptrQueue, 8) {
	    printf("Scanning some pointer. %d\n", i);
	    alloc_pointer = scanptr(ptrQueue[i], alloc_pointer);    
	  }
	}
      }
    }
    
    random_iter(i, ptrQueue, ptrQueueIdx) {
      printf("Scanning some pointer. %d\n", i);
      alloc_pointer = scanptr(ptrQueue[i], alloc_pointer);    
    }
  }

  // scan saved objects from roots
  {
    void** scan_start = local_heap.alloc_ptr;
    printf("Scanning heap: start %p\n", scan_start);
    for(void** scan_pointer = scan_start; scan_pointer < (void**)alloc_pointer; scan_pointer++) {
      void** block_start = scan_pointer;
      void** block_end = (void**)alloc_pointer;
      //printf("Scanning block with length %ld. S\n", (uintptr_t)(block_end-block_start));
      random_iter(i, block_start, (uintptr_t)(block_end-block_start)) {
	printf("Scanning block with length %ld. %d\n", (uintptr_t)(block_end-block_start), i);
	alloc_pointer = scanptr(block_start+i, alloc_pointer);
      }
      scan_pointer = block_end;
    }
    printf("Done scanning heap: stop %p\n", alloc_pointer);
  }

  local_heap.alloc_ptr = alloc_pointer;  

  local_heap.state = STATE_DONE;
  __sync_synchronize();

  printf("%p: Waiting for all others to finish\n", &local_heap);
  if(master) {
    for(int i=0; i < MAX_THREADS; i++) {
      local_heap_t* t = heap.thread_heap[i];
      if( t != NULL && t != &local_heap ) {
	while(t->state != STATE_DONE);
      }
    }

    heap.collection_needed = STATE_NOT_COLLECTING;
  } else {
    while(heap.collection_needed != STATE_NOT_COLLECTING);
  }

  printf("%p: Collection done\n", &local_heap);
  local_heap.state = STATE_MUTATING;

  return 0;
}

void alloc_printstat() {
  printf("Heap: %p (size %ld)  ", heap.currentspace, heap.currentspace_end-heap.currentspace);
  printf("local heap: %ld free\n", local_heap.alloc_end - local_heap.alloc_ptr);
}

void alloc_safe_point() {
  if(heap.collection_needed == STATE_COLLECTION_NEEDED) {
    alloc_collect();
  }
}

void* alloc(unsigned long int n) {
  alloc_safe_point();

  void* p = local_heap.alloc_ptr + TAG_SIZE;
  p = (void*)(((uintptr_t)p + alloc_align) & ~(alloc_align-1));

  local_heap.alloc_ptr = p + n;
  if(local_heap.alloc_ptr > local_heap.alloc_end) {
    alloc_collect();
    return alloc(n);
  }

  TAG_OF_PTR(p) = TAG_FROM_SIZE(n);

  return p;
}
