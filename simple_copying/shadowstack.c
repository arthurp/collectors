#include "shadowstack.h"
#include <stdlib.h>


__thread shadow_stack_frame* shadow_stack_top = NULL;
