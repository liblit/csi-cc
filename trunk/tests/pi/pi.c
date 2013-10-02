// NOTE: this example adopted from
// http://www.dartmouth.edu/~rc/classes/soft_dev/C_simple_ex.html

#include <stdio.h>

#include "rand.h"
#define RAND_MAX 10000

int main()
{
  int niter=0;
  double x,y;
  int i,count=0; /* # of points in the 1st quadrant of unit circle */
  double z;
  double pi;
  
  printf("Enter the number of iterations used to estimate pi: ");
  scanf("%d",&niter);
  
  count=0;
  for (i = 0; i < niter; i++){
    x = (double)rand(RAND_MAX)/RAND_MAX;
    y = (double)rand(RAND_MAX)/RAND_MAX;
    z = x*x+y*y;
    if(z <= 1)
      count++;
  }
  pi=(double)count/niter*4;
  printf("# of trials= %d , estimate of pi is %g \n",niter,pi);
}
