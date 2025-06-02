/* 1/16/2002 Joe Berens (SGI) */

/* Why does this not fail */

void main(void) {
  char *ptr;
  int i,sizeOf;
  int Gb = 1024*1024*1024;

  sizeOf = sizeof(char);
  printf("sizeof char %d\n\n", sizeOf);
 
  ptr = (char *)malloc(sizeof(char) * Gb);
    
  for (i = 0; i < Gb; i++) {  
    ptr = "a";
    if (i%(1024*1024) == 0) 
      printf("%d Mb written\n", i/(1024*1024));
    ptr++;
  }
}
