#include "randomize.h"

__thread int seed;

int __rand(int s) {
  seed >>= 1;
  seed += (s>>5)*7459;
  seed = seed*1664525 + 1013904223;
  
  return (seed>>4) & 0x7fffffff;
}
