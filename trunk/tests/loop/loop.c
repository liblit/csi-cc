#include <stdio.h>

int rand(){
  static int x = 3; 
  return(x = (x * 8121 + 28411) % 134455);
}

int main(){
  int x = rand()%14;
  while(x!=2){
    printf("ANSWER: %d\n", x);
    int y = rand()%2;
    if(y==1){
      printf("Y= %d\n", 1);
    }
    else
      printf("Y= %d\n", 0);
    x = rand()%14;
  }
  printf("DONE: %d\n", x);
}
