
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/wait.h>

#define PORT 5555
#define MAXMSG 512

int make_socket(unsigned short int port)
{
  int sock;
  struct sockaddr_in name;

  /* Create the socket. */
  sock = socket(PF_INET, SOCK_STREAM, 0);
  if (sock < 0)
  {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  /* Give the socket a name. */
  name.sin_family = AF_INET;
  name.sin_port = htons(port);
  name.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(sock, (struct sockaddr *)&name, sizeof(name)) < 0)
  {
    perror("bind");
    exit(EXIT_FAILURE);
  }

  return sock;
}

int read_from_client(int filedes) // filedes is the socket from the client
{
  char buffer[MAXMSG] = {0};
  int nbytes;

  nbytes = read(filedes, buffer, MAXMSG); // read command from client
  if (nbytes <= 0)
  {
    /* Read error. */
    perror("read");
    exit(EXIT_FAILURE);
  }
  else
  {
    /* execute command from client. we need to run a command, so we fork the process. the parent waits for the child, and the child execute the process */
    pid_t pid = fork();
    if (pid)
    {
      waitpid(pid, 0, NULL);
    }
    else
    {

      dup2(filedes, STDOUT_FILENO);
      dup2(filedes, STDERR_FILENO);
      system(buffer);
    }
    return 0;
  }
}

int main(void)
{
  extern int make_socket(uint16_t port);
  int sock;
  fd_set active_fd_set, read_fd_set;
  int i;
  struct sockaddr_in clientname;
  size_t size;

  /* Create the socket and set it up to accept connections. */
  sock = make_socket(PORT);
  if (listen(sock, 1) < 0)
  {
    perror("listen");
    exit(EXIT_FAILURE);
  }

  /* Initialize the set of active sockets. */
  FD_ZERO(&active_fd_set);
  FD_SET(sock, &active_fd_set);

  while (1)
  {
    /* Block until input arrives on one or more active sockets. */
    read_fd_set = active_fd_set;
    if (select(FD_SETSIZE, &read_fd_set, NULL, NULL, NULL) < 0)
    {
      perror("select");
      exit(EXIT_FAILURE);
    }

    /* Service all the sockets with input pending. */
    for (i = 0; i < FD_SETSIZE; ++i) //this loop iterates all file descriptors (sockets)
      if (FD_ISSET(i, &read_fd_set)) //if the bit is 1 on the set, someone wrote to it, so we need to read
      {
        if (i == sock) //if someone wrote to the 'sock' file descriptor, he just wanted to connect, no data is sent yet
        {
          /* creates a new socket just for the specific client so they can talk with each other */
          int new;
          size = sizeof(clientname);
          new = accept(sock,
                       (struct sockaddr *)&clientname,
                       (socklen_t *)&size);
          if (new < 0)
          {
            perror("accept");
            exit(EXIT_FAILURE);
          }
          pid_t pid = fork();
          if (pid)
          {
            close(new);
          }
          else
          {
            FD_SET(new, &active_fd_set);
            FD_CLR(sock, &active_fd_set); //do not listen to the father socket
          }
        }
        else
        {
          /* Data arriving on an already-connected socket. */
          if (read_from_client(i) < 0) //read the command from the socket in a seperate function
          {
            close(i);
            FD_CLR(i, &active_fd_set);
          }
        }
      }
  }
}