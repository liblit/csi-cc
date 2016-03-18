#include <stdio.h>

int foo(){
  return 3;
}

int bar(){
  return 4;
}

int main(int argc, char** argv){
  (void)argv;
  int (*fn)();
  if(argc < 2)
    fn = foo;
  else
    fn = bar;
  int myval = fn();
  printf("%d\n", myval);
}
