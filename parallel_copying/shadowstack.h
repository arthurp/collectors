#pragma once

#include <stdint.h>

typedef struct shadow_stack_frame_tag shadow_stack_frame;

typedef struct shadow_stack_frame_tag {
  shadow_stack_frame* prev;
  uintptr_t length;
  void* elements[];
} shadow_stack_frame;


#define SHADOW_FRAME(name, n)						\
  void* vp_shadow_stack_frame_##name [n+2];				\
  shadow_stack_frame* shadow_stack_frame_##name = (shadow_stack_frame*)vp_shadow_stack_frame_##name; \
  shadow_stack_frame* old_shadow_stack_top_##name = shadow_stack_top;	\
  shadow_stack_frame_##name ->prev = old_shadow_stack_top_##name ;	\
  shadow_stack_frame_##name ->length = n
#define SHADOW_VAR(name, i, v) shadow_stack_frame_##name ->elements[i] = &(v)
#define SHADOW_END(name) shadow_stack_top = shadow_stack_frame_##name

#define SHADOW_POP_FRAME(name) shadow_stack_top = old_shadow_stack_top_##name

extern __thread shadow_stack_frame* shadow_stack_top;

