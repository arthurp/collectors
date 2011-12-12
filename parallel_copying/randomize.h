#pragma once

int __rand(int s);

#define random_iter(idx, ptr, limit)					\
  int __rnd = __rand((int)(uintptr_t)ptr);				\
  for(int idx, cnt = __rnd; idx = cnt%limit, cnt != limit+__rnd; cnt++) 

// { val_decl = ptr[(__idx+__rnd) % limit];
// #define random_iter_end() } 

