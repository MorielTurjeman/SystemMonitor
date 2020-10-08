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
#include <execinfo.h>
#include <stdbool.h>

#define PORT 5555
#define MAXMSG 512
#define BUFSIZE (100 * (sizeof(struct inotify_event) + NAME_MAX + 1))
#define BT_BUF_SIZE 100

struct cli_def *cli;

typedef struct
{
    char *path;
    char *ip;
    char *port;
} ThreadParams;

static bool backtrace_requested = false;
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mtxString = PTHREAD_MUTEX_INITIALIZER;
int nptrs;
char **strings;
int sock;

void sighandler(int sig)
{
    if (sig == SIGKILL || sig == SIGTERM)
    {
        shutdown(sock, SHUT_RDWR);
    }

}


void print_backtrace(void)
{

    int s;
    int ms;
    s = pthread_mutex_lock(&mtx);
    ms = pthread_mutex_lock(&mtxString);

    if (s != 0 || ms != 0) // if the mtx unlocked
    {
        perror("pthread_mutex_lock");
    }
    if (backtrace_requested)
    {

        int j;
        void *buffer[BT_BUF_SIZE];

        nptrs = backtrace(buffer, BT_BUF_SIZE); //get list "addresses" which led us to this point

        /* The call backtrace_symbols_fd(buffer, nptrs, STDOUT_FILENO)
              would produce similar output to the following: */

        strings = backtrace_symbols(buffer, nptrs); //try to qconvert address list to function names
        if (strings == NULL)
        {
            perror("backtrace_symbols");
            exit(EXIT_FAILURE);
        }

        backtrace_requested = false;
    }

    ms = pthread_mutex_unlock(&mtxString);
    s = pthread_mutex_unlock(&mtx);

    if (s != 0 || ms != 0)
    {
        perror("pthread_mutex_unlock");
    }
}

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

    if ((access & IN_ISDIR))
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

    snprintf(buf2, 256, "FILE ACCESSED: %s/%s\nACCESS: %s\nTIME OF ACCESS: %s\n", filePath, fileName, (access & IN_CLOSE_NOWRITE) ? "NO_WRITE" : "WRITE", buffer); // print to

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

int backtraceHandler(struct cli_def *cli, const char *command, char *argv[], int argc)
{

    int s;
    int ms;

    s = pthread_mutex_lock(&mtx);
    if (s != 0) // if the mtx unlocked
    {
        perror("pthread_mutex_lock");
    }

    backtrace_requested = true;
    s = pthread_mutex_unlock(&mtx);
    if (s != 0) // if the mtx unlocked
    {
        perror("pthread_mutex_ulock");
    }
    while (true)
    {
        pthread_mutex_lock(&mtxString);
        if (strings != NULL)
            break;
        pthread_mutex_unlock(&mtxString);
    }
        

    for (int j = 0; j < nptrs; j++)
        cli_print(cli, "%s", strings[j]);

    free(strings);
    strings = NULL;
    ms = pthread_mutex_unlock(&mtxString);

    
    return CLI_OK;
}

int read_from_client(int filedes)
{

    struct cli_command *c = cli_register_command(cli, NULL, "backtrace", backtraceHandler, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, NULL);
    cli_loop(cli, filedes);
}

void *serverMonitor(void *params)
{
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
                        read_from_client(new);
                    }
                }
            }
        }
    }
}

int main(int argc, char  *argv[])
{

    cli = cli_init();
    pthread_t t;
    pthread_t cliThread;
    ThreadParams tp;
    int opt;

    if (argc != 5)
        exit(-1);

    while ((opt = getopt(argc, argv, "d:i:")) != -1)
    {
        switch (opt)
        {
            case 'd':
                tp.path = optarg;
                break;
            case 'i':
                tp.ip = optarg;
                break;
        }   
    }


    tp.port = "9000";

    int res = pthread_create(&t, NULL, systemMonitor, &tp);
    int res2 = pthread_create(&cliThread, NULL, serverMonitor, NULL);

    pthread_join(t, NULL);
    signal(SIGKILL, sighandler);
    signal(SIGTERM, sighandler);
    return 0;
}

void __attribute__((no_instrument_function)) __cyg_profile_func_enter(void *this_fn,
                                                                      void *call_site)
{
    if (this_fn == systemMonitor || this_fn == netcatLog || this_fn == buildNC)
        print_backtrace();
}

void __attribute__((no_instrument_function)) __cyg_profile_func_exit(void *this_fn,
                                                                     void *call_site)
{
}