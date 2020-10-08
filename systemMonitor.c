#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <libcli.h>

#define PORT 5555
#define MAXMSG 512
#define BUFSIZE (100 * (sizeof(struct inotify_event) + NAME_MAX + 1))


struct cli_def* cli;




typedef struct
{
    char *path;
    char *ip;
    char *port;
} ThreadParams;

int buildNC(char *ip, char *port)
{
    int p[2]; // entrance and exit
    pipe(p);
    pid_t pid = fork();

    if (pid)
    {
        close(p[0]);
        // dup2(p[1], STDOUT_FILENO);
        return p[1];
    }
    else
    {
        close(p[1]);
        dup2(p[0], STDIN_FILENO); //close file descriptor 0 (STDIN) and copies
        if (execlp("nc", "nc", "-uN", "-w0", ip, port, NULL))
            printf("%s", strerror(errno));
    }
}

void netcatLog(char *filePath, char *fileName, int access, ThreadParams *tp) // maybe later add the file that i want to pass the data, like html
{
    // struct stat sb;
    // char fullPath[256] = {0};
    //     strcat(fullPath, filePath);

    // if (fileName[0] != 0)
    // {
    //     strcat(fullPath,"/");
    //     strcat(fullPath, fileName);

    // }

    // printf("%s\n", fullPath);
    // if (stat(fullPath, &sb) == -1)
    // {
    //     perror("stat");
    //     exit(EXIT_FAILURE);
    // }

    if ((access & IN_ISDIR) && (access & IN_CLOSE_NOWRITE))
        return;

    int nc_fd = buildNC(tp->ip, tp->port);

    time_t rawtime;
    // time_t rawtime = sb.st_atim.tv_sec;
    struct tm *timeinfo;
    char buf2[256];

    char buffer[80];

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    strftime(buffer, 80, "%d %B %G %R", timeinfo);

    snprintf(buf2, 256, "FILE ACCESSED: %s/%s\n ACCESS: %s %x\n TIME OF ACCESS: %s\n", filePath, fileName, (access & IN_CLOSE_NOWRITE) ? "NO_WRITE" : "WRITE", access, buffer); // print to

    write(nc_fd, buf2, strlen(buf2));
    fsync(nc_fd);
    close(nc_fd);

    FILE *htmlPath = fopen("/var/www/html/index.html", "a");
    fprintf(htmlPath, "FILE ACCESSED: %s/%s<br> ACCESS: %s<br> TIME OF ACCESS: %s<br>", filePath, fileName, (access & IN_CLOSE_NOWRITE) ? "NO_WRITE" : "WRITE", buffer);
    fclose(htmlPath);
}

void *systemMonitor(void *params)
{

    ThreadParams *tp = (ThreadParams *)params;
    char *watchname = tp->path;
    char *ip = tp->ip;
    int notifyFd, watchfd;
    struct inotify_event *event;
    char eventbuff[BUFSIZE];
    int n;
    char *p;

    notifyFd = inotify_init(); // init fd to listen
    if (notifyFd < 0)
    {
        printf("%s", strerror(errno));
        return NULL;
    }

    watchfd = inotify_add_watch(notifyFd, watchname, IN_CLOSE_NOWRITE | IN_CLOSE_WRITE);
    if (watchfd < 0)
    {
        printf("%s", strerror(errno));
        return NULL;
    }

    while (1)
    {
        n = read(notifyFd, eventbuff, BUFSIZE); // read the events into the eventbuf

        for (p = eventbuff; p < eventbuff + n;)
        {
            event = (struct inotify_event *)p;
            p += sizeof(struct inotify_event) + event->len;
            netcatLog(tp->path, event->name, event->mask, tp);

            fflush(stdout);
        }
    }

    return NULL;
}

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


int read_from_client(int filedes)
{   

   struct cli_command *c=cli_register_command(cli ,NULL, "backtrace", NULL, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, NULL);
   cli_set_banner(cli, "hello world");
   cli_loop(cli, filedes);

        
}




void *serverMonitor(void *params)
{
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

        for (i = 0; i < FD_SETSIZE; ++i)
        {
            if (FD_ISSET(i, &read_fd_set))
            {
                if (i == sock)
                {

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

                    else
                    {
                        FD_SET(new, &active_fd_set);
                        FD_CLR(sock, &active_fd_set);
                    }
                }
                else
                {
                    if (read_from_client(i) < 0) //read the command from the socket in a seperate function
                    {
                        close(i);
                        FD_CLR(i, &active_fd_set);
                    }
                }
            }
        }
    }
}

int main(int argc, char const *argv[])
{
    
    cli =cli_init();
    pthread_t t;
    pthread_t cliThread;
    ThreadParams tp;
    tp.path = "/home/moriel99/code/blalba";
    tp.ip = "192.168.1.115";
    tp.port = "9000";

    int res = pthread_create(&t, NULL, systemMonitor, &tp);
    int res2= pthread_create(&cliThread,NULL,serverMonitor,NULL);

    pthread_join(t, NULL);
    return 0;
}
