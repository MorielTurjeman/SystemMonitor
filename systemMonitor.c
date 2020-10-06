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
#define BUFSIZE (100 * (sizeof(struct inotify_event) + NAME_MAX + 1))

typedef struct
{
    char *path;
    char *ip;
    char *port;
} ThreadParams;

void buildNC(char *ip, char *port)
{
    int p[2]; // entrance and exit
    pipe(p);
    pid_t pid = fork();

    if (pid)
    {
        close(p[0]);
        dup2(p[1], STDOUT_FILENO);
    }
    else
    {
        close(p[1]);
        dup2(p[0], STDIN_FILENO);
        if (execlp("nc", "nc", "-u", ip, port, NULL))
            printf("%s", strerror(errno));
    }

    return;
}

void *systemMonitor(void *params)
{
    

    ThreadParams *tp = (ThreadParams *)params;
    char *watchname = tp->path;
    char *ip = tp->ip;
    FILE *dest; // write all the changes at the directory to this file
    int notifyFd, watchfd;
    struct inotify_event *event;
    char eventbuff[BUFSIZE];
    int n;
    char *p;

    buildNC(tp->ip, tp->port);

    notifyFd = inotify_init(); // init fd to listen
    if (notifyFd < 0)
    {
        printf("%s", strerror(errno));
        return NULL;
    }

    watchfd = inotify_add_watch(notifyFd, watchname, IN_ATTRIB | IN_CREATE | IN_DELETE | IN_DELETE_SELF | IN_MODIFY | IN_MOVE_SELF);
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
            if (event->mask & IN_ACCESS)
                printf("%s file was accessed\n", event->name);
            if (event->mask & IN_ATTRIB)
                printf("%s file attributes changed\n", event->name);
            if (event->mask & IN_CREATE)
                printf("%s file created inside watched directory\n", event->name);
            if (event->mask & IN_DELETE)
                printf("%s file deleted inside watched directory\n", event->name);
            if (event->mask & IN_DELETE_SELF)
                printf("watched file %s  deleted\n", event->name);
            if (event->mask & IN_MODIFY)
                printf("%s file was modified\n", event->name);
            if (event->mask & IN_MOVE_SELF)
                printf("%s file  was moved\n", event->name);
            
            fflush(stdout);
        }
    }

    return NULL;
}

int main(int argc, char const *argv[])
{
    pthread_t t;
    ThreadParams tp;
    tp.path = "/home/moriel99/code/unix/project";
    tp.ip = "192.168.1.115";
    tp.port="9000";

    int res = pthread_create(&t, NULL, systemMonitor, &tp);

    pthread_join(t, NULL);
    return 0;
}
