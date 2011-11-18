#include "alloc.h"

#include <gc/gc.h>
#include <stdio.h>
#include <stdint.h>
#include <malloc.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>

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


__thread void* stack_base;

void* currentspace;
void* alloc_pointer;
void* currentspace_end;

void* nextspace;
void* nextspace_end;

int alloc_init(size_t heap_size) {

  // Find the stack.
  struct GC_stack_base stb;
  int ret = GC_get_stack_base(&stb);
  stack_base = stb.mem_base;
  printf("Stack base is %p (current frame is around %p)\n", stack_base, &ret);

  // Allocate our heap.
  int pagesize = getpagesize();
  heap_size = heap_size + (pagesize - heap_size % pagesize);
  currentspace = memalign(pagesize, heap_size*2);
  currentspace_end = currentspace + heap_size;

  nextspace = currentspace + heap_size;
  nextspace_end = nextspace + heap_size;

  alloc_pointer = currentspace;
  printf("Current space is  %p with size 0x%lx\n", currentspace, heap_size);
  printf("Next space is  %p with size 0x%lx\n", nextspace, heap_size);

  return ret;
}

inline static void* scanptr(void** p, void* next_alloc_pointer) {
  void* candidate = *p;
  if(candidate >= currentspace && candidate < currentspace_end) {
    //printf("Found pointer on stack: %p at %p\n", candidate, p);
    
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

      assert(!IS_FORWARDING_PTR(TAG_OF_PTR(newptr)));
      
      next_alloc_pointer = newptr + SIZE_FROM_TAG(tag);
      
      TAG_OF_PTR(candidate) = FORWARDING_PTR_OF_PTR(newptr); 
    }
  }

  return next_alloc_pointer;
}

int alloc_collect() {
  //char dummy;

  Force GCC to spill all registers.

  // scan stack
  void* stack_start = __builtin_frame_address(0); //&dummy + 1; 
  void* stack_end = stack_base;

  assert(stack_end > stack_start);
  assert((uintptr_t)stack_end % sizeof(void*) == 0);
  assert((uintptr_t)stack_start % sizeof(void*) == 0);

  printf("Scanning stack: %p - (%p) %p\n", stack_start, &stack_start, stack_end);

  void* next_alloc_pointer = nextspace;

  for(void** p = stack_start; p < (void**)stack_end; p++) {
    next_alloc_pointer = scanptr(p, next_alloc_pointer);
  }

  // scan saved objects from roots
  printf("Scanning heap: start %p\n", nextspace);
  for(void** scan_pointer = nextspace; scan_pointer < (void**)next_alloc_pointer; scan_pointer++) {
    next_alloc_pointer = scanptr(scan_pointer, next_alloc_pointer);
  }
  printf("Done scanning heap: stop %p\n", next_alloc_pointer);
  
  // swap spaces
  void* tmp;
  tmp = nextspace;
  nextspace = currentspace;
  currentspace = tmp;
  tmp = nextspace_end;
  nextspace_end = currentspace_end;
  currentspace_end = tmp;

  alloc_pointer = next_alloc_pointer;

  return 0;
}

void alloc_printstat() {
  printf("Heap: %p (size %ld)  ", currentspace, currentspace_end-currentspace);
  printf("%ld allocated, %ld free\n", alloc_pointer-currentspace, currentspace_end-alloc_pointer);
}

void* alloc(unsigned long int n) {
  void* p = alloc_pointer + TAG_SIZE;
  p = (void*)(((uintptr_t)p + alloc_align) & ~(alloc_align-1));
  TAG_OF_PTR(p) = TAG_FROM_SIZE(n);

  alloc_pointer = p + n;
  assert(alloc_pointer < currentspace_end);
  return p;
}
