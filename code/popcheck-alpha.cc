// 10/17/2001 Joe Berens jberens@sgi.com (SGI) 

#include <ctype.h>
#include <crypt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <iostream.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <wait.h>

#define PORT 110
#define BUFSIZE 1024

// global variables
char *servername;
char *userusername;
char *passpassword;

// function prototypes
void init(void);
int getMsgCount(void);
void usage(char *arg);

// initialize global variables
void init(int argc, char **argv) {  
  char *username;
  char *password;
  char *prompt;
  uid_t uid;
  passwd *pw;
  
  // initialize <servername>
  servername = argv[1];
  
  // initialize <username>
  if (argc == 2) {
    uid = getuid();
    pw =  getpwuid(uid);
    username = pw->pw_name; 
  }
  else {
    username =  argv[2];
  }
  
  // build "send <username>"
  userusername = (char *)malloc(sizeof(char) * (strlen(username) + 6));
  strcpy(userusername, "user ");
  strcat(userusername, username);
  strcat(userusername, "\n");
 
  
  // build prompt
  prompt = (char *)malloc(sizeof(char) * \
			  strlen("enter password ") +
			  strlen("for ") +
			  strlen(username) +
			  strlen("@") +
			  strlen(servername) +
			  strlen(": "));
  strcat(prompt, "enter password ");
  strcat(prompt, "for ");
  strcat(prompt, username);
  strcat(prompt, "@");
  strcat(prompt, servername);
  strcat(prompt, ": ");
  
  // initialize <password> 
  // get <password> from command line if both username and password are there
  if (argc == 4) 
    password = argv[3];
  else {
    password = (char *)malloc(sizeof(char) * 15);
    password = getpass(prompt);
    delete prompt;
  }
  
  // build "pass <password>"
  passpassword =  (char *)malloc(sizeof(char) * (strlen(password) + 6));
  strcpy(passpassword, "pass ");
  strcat(passpassword, password);
  strcat(passpassword, "\n");
  delete password;
}

// find out how many messages are on the server
int getMsgCount() {
  
  int i, j, sd;
  char buf[BUFSIZE];
  struct sockaddr_in pin;
  struct hostent *hp;

  // go find out about the desired host machine 
  if ((hp = gethostbyname(servername)) == 0) {
    perror("gethostbyname");
    return(-1);
  }
  
  // fill in the socket structure with host information 
  memset(&pin, 0, sizeof(pin));
  pin.sin_family = AF_INET;
  pin.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr))->s_addr;
  pin.sin_port = htons(PORT);
  
  // grab an Internet domain socket 
  if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    perror("socket");
    return(-1);
  }
  
  // connect to PORT on <servername> 
  if (connect(sd, (struct sockaddr *)  &pin, sizeof(pin)) == -1) {
    perror("connect");
    return(-1);
  }
  
  // wait for "+OK POP3 , <servername> <version> server ready"
  // message to come back from the server 
  if ((i = recv(sd, buf, BUFSIZE, 0)) == -1) {
    perror("recv");
    return(-1);
  }

  // clear the buffer
  for (j = 0; j < i; j++)
    buf[j] = 0;
  
  // send "user <username>" to the server PORT on <servername> 
  if (send(sd, userusername, strlen(userusername), 0) == -1) {
    perror("send user <username>");
    return(-1);
  }
  
  // wait for "+OK User name accepted, password please" 
  // message to come back from the server 
  if ((i = recv(sd, buf, BUFSIZE, 0)) == -1) {
    perror("recv");
    return(-1);
  }
  
  // clear the buffer
  for (j = 0; j < i; j++)
    buf[j] = 0;
  
  // send "pass <password>" to the server PORT on <servername> 
  if (send(sd, passpassword, strlen(passpassword), 0) == -1) {
    perror("send pass <password>");
    return(-1);
  }
  
  // wait for "+OK Mailbox open, <n> messages"
  // message to come back from the server 
  if ((i = recv(sd, buf, BUFSIZE, 0)) == -1) {
    perror("recv");
    return(-1);
  }
  
  if(strncmp(buf, "-", 1) == 0) {
    perror("password and/or username incorrect, try again.");
    return(-1);
  }
    
  // clear the buffer
  for (j = 0; j < i; j++)
    buf[j] = 0;
  
  // send "stat" to the server PORT on <servername> 
  if (send(sd, "stat\n", strlen("stat")+1, 0) == -1) {
    perror("send stat");
    return(-1);
  }
  
  // wait for "+OK <n> <m>"
  // message to come back from the server 
  if ((i = recv(sd, buf, BUFSIZE, 0)) == -1) {
    perror("recv");
    return(-1);
  }
  
  // convert the count to an int
  char count[100];
  i = 0;
  j = 0;
  while (!isdigit(buf[i])) 
    i++;
  while (isdigit(buf[i])) {
    count[j] = buf[i];
    i++;
    j++;
  }
  
  // clear the buffer
  for (j = 0; j < i; j++)
    buf[j] = 0;
  
  // send "quit" to the server PORT on <servername> 
  if (send(sd, "quit\n", strlen("quit")+1, 0) == -1) {
    perror("send stat");
    return(-1);
  }
  
  // wait for "+OK <message>"
  //  message to come back from the server 
  if ((i = recv(sd, buf, BUFSIZE, 0)) == -1) {
    perror("recv");
    return(-1);
  }
  
  //delete buf;
  close(sd);
  delete &sd;
  delete &pin;
  return(atoi(count)); 
}

void usage(char *arg) {
  cout << "Usage: " << arg << " <server>" << " [<username>]" << endl;
  cout << "   or: " << 
    arg << " <server>" << " <username> " << "<password>" << endl; 
}

int main(int argc, char **argv) {
  int msgcount, tmpcount;
  char hostname[100];
  bool notified = false;
  pid_t pid;

  if (argc < 2 || argc > 3) {
    usage(argv[0]);
    exit(-1);
  }
  gethostname(hostname, 100);
  init(argc, argv);
  msgcount = getMsgCount();
  if(msgcount == -1) {
    execv(argv[0], argv);
  }
  pid = fork();
  if (pid > 0) {
    exit(0);
  }
  else if (pid == -1) {
    perror("coud not fork");
    exit(-1);
  }
  else {
    while (1) {
      if (notified) {
	sleep(120);
	notified = false;
      }
      else 
	sleep(300);
      tmpcount = getMsgCount();
      if (tmpcount > msgcount) {
	msgcount = tmpcount;
	int statloc;
	pid = fork();
	if (pid == 0) {
	  execl("/usr/bin/X11/xmessage", "-", "you have mail", (char*)0);
	  notified = true;
	  sleep(180);
	  execl("/sbin/killall", "xmessage", (char*)0);
	}
	else if(pid > 0)
	  waitpid(-1, &statloc, WNOHANG);
	else if(pid == -1) 
	  exit(1);
      } 
    }
  } 
}

