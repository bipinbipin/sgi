// 10/17/2001 Joe Berens jberens@sgi.com (SGI) 

#include <ctype.h>
#include <crypt.h>

#include <netdb.h>
#include <netinet/in.h>

#include <iostream.h>
#include <limits.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <wait.h>

#define BUFSIZE 1024

class MailChecker {
  
private:
  int port;
  char *servername;
  char *userusername;
  char *passpassword;
  char *username;
  char *password;
  char *prompt;
  uid_t uid;
  passwd *pw;

public:
  MailChecker(int argc, char **argv) {
    servername = argv[1];
    
    if (argc == 2) {
      uid = getuid();
      pw =  getpwuid(uid);
      username = pw->pw_name; 
    }
    else {
      username =  argv[2];
    }    
    userusername = (char *)malloc(sizeof(char) * (strlen(username) + 6));
    strcpy(userusername, "user ");
    strcat(userusername, username);
    strcat(userusername, "\n");
   
    if (argc == 4) 
      password = argv[3];

    else { 
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
      
      password = (char *)malloc(sizeof(char) \
                                * ((strlen("enter password for "))
                                   + strlen(username) 
                                   + strlen("@")
                                   + strlen(servername)
                                   + strlen(": ")));
      password = getpass(prompt);
      delete prompt;
    }
    passpassword =  (char *)malloc(sizeof(char) 
                                   * (strlen("pass ") +(strlen(password))));
    strcpy(passpassword, "pass ");
    strcat(passpassword, strdup(password));
    strcat(passpassword, "\n");
    //delete password;
  }
  
  int getMsgCount() {
    int i, j, sd;
    char buf[BUFSIZE];
    struct sockaddr_in pin;
    struct hostent *hp;
    struct servent *se;
    
    // go find out about the desired host machine 
    hp  = gethostbyname("poppy");
    if ((hp = gethostbyname(servername)) == 0) {
      perror("gethostbyname");
      return(-2);
    }
    
    // go find out about the port for tcp/pop3
    if ((se = getservbyname("pop3", "tcp")) == 0) {

      perror("getservbyname");
      return(-4);
    }
    port = se->s_port;
    
    // fill in the socket structure with host information 
    memset(&pin, 0, sizeof(pin));
    pin.sin_family = AF_INET;
    pin.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr))->s_addr;
    pin.sin_port = htons(port);
    
    // grab an Internet domain socket 
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
      perror("socket");
      return(-1);
    }
    
    // connect to port on <servername> 
    if (connect(sd, (struct sockaddr *)  &pin, sizeof(pin)) == -1) {
      perror("connect");
      return(-3);
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
    
    // send "user <username>" to the server port on <servername> 
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
    
    // send "pass <password>" to the server port on <servername> 
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
   
    if (strncmp(buf, "-", 1) == 0) {
      perror("password and/or username incorrect, try again.");
      return(-1);
    }
    
    // clear the buffer
    for (j = 0; j < i; j++)
      buf[j] = 0;
    
    // send "stat" to the server port on <servername> 
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
    
    // send "quit" to the server port on <servername> 
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
    //delete &sd;
    //delete &pin; 
    return(atoi(count)); 
  }
};

int main(int argc, char **argv) {
  if (argc < 2 || argc > 4) {
    cerr << "Usage: " << argv[0] << " <server>" << " [<username>]" << endl;
    cerr << "   or: " << 
      argv[0] << " <server>" << " <username> " << "<password>" << endl; 
    exit(-1);
  }
  
  int msgcount, tmpcount;
  pid_t pid;
  bool notified = false;
  MailChecker *mc = new MailChecker(argc, argv);
  
  msgcount = mc->getMsgCount();
  
  if (msgcount == -1) {
    execv(argv[0], argv);
  }
  if (msgcount == -2) {
    exit(-1);
  }
  if (msgcount == -3) {
    exit(-1);
  }
  pid = fork();
  if (pid == 0) {
    while (1) {
      if (notified)
        sleep(75);
      else 
        sleep(150);
      tmpcount = mc->getMsgCount();
      if (tmpcount > msgcount) {
        msgcount = tmpcount;
        int statloc;
        pid = fork();
        if (pid > 0) {
          waitpid(-1, &statloc, WNOHANG);
          pid = fork();
          if (pid > 0) {
            waitpid(-1, &statloc, WNOHANG); 
          }
          else if (pid == 0) {
            notified = false;
            sleep(150);
            execl("/sbin/killall", "", "xmessage", (char*)0);
          }
          else if (pid == -1) {

            perror("could not fork");
            exit(-1);
          }
        }
        else if (pid == 0) {
          notified = true;
          execl("/usr/bin/X11/xmessage", "Mail Notifier", \
                "you have new mail on", argv[1], (char*)0);
        }
        else if (pid == -1) {
          perror("could not fork");
          exit(-1);
        }
      }
      msgcount = tmpcount;
    }
  }
  else if (pid > 0) {
    exit(0);
  }
  else if (pid == -1) {
    perror("coud not fork");
    exit(-1);
  } 
}


