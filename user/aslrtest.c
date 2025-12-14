#include "kernel/types.h"
#include "user/user.h"

int y;

int main(int argc, char *argv[])
{
  int x;
  int *ptr;
  x = 10;
  y = 20;
  ptr = (int *)malloc(4);
  printf("main! :%p\n", main);
  printf("Stack : x address! :%p\n", &x);
  printf("Stack y: address! :%p\n", &y);
  printf("heap address!: %p\n", ptr);
  exit(1);
}