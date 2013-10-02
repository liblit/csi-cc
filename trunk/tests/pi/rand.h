int rand(int lim){
  static int x = 3; 
  return(x = (x * 8121 + 28411) % lim);
}
