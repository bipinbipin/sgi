#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>


main()
{

	int pid_t;
	int i=0;
	const char *path = "/usr/sbin/xwsh";
	char *arg0 = "xwsh";
	char *arg1 = "-fg";
	char *arg2 = "#000000";
	char *arg3 = "-bg";
	char *arg4 = "#ffffff";
	
	
	
	if (pid_t == 0)
	  { pid_t = fork(); }

	else 
	  {
	    i++;
	    arg2[i] = 'f';
	    pid_t = fork();
	    execlp(path,arg0,arg1,arg2,arg3,arg4,(char *)0);
	  }

	

}
