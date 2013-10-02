#include <stdio.h>

int abc(int x){
  if(x < 10){
    return x + 4;
  }
  else
    return x - 3;
}

int a(int* p){
  int x = *p%2;
  if(x==1)
    printf("ANSWER: %d\n", 1);
  else
    x++;
  x = abc(x);
  x = abc(x);
  x = abc(x);
  printf("X is now: %d\n", x);
  return x + *p;
}

int b(int* p){
  if(*p%467 != 0)
    return(a(p));
  return 145;
}

int main(){
  int x = 3;
  int q = 12;
  int* p = &x;
  int* qp = &q;
  printf("First return was: %d\n", a(p));
  printf("Second return was: %d\n", b(qp));
}
