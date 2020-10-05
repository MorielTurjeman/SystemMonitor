#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <limits.h>
#include <errno.h>
#include<unistd.h>
#define BUFSIZE (100*(sizeof(struct inotify_event)+NAME_MAX+1))




void systemMonitor(char* watchname, char* ip)
{
    FILE *dest;                     // write all the changes at the directory to this file
    int notifyFd, watchfd;
    struct inotify_event *event;
    char eventbuff[BUFSIZE];
    int n;
    char *p;


    notifyFd = inotify_init(); // init fd to listen
    if (notifyFd < 0)
    {
        printf("%s", strerror(errno));
        return -1;
    }

    watchfd = inotify_add_watch(notifyFd, watchname, IN_ATTRIB | IN_CREATE | IN_DELETE | IN_DELETE_SELF | IN_MODIFY | IN_MOVE_SELF);
     if(watchfd<0)
     {
         printf("%s", strerror(errno));
        return -1;
     }

    
    while(1)
    {
        n= read(notifyFd,eventbuff, BUFSIZE); // read the events into the eventbuf

        for(p=eventbuff; p<eventbuff+n;)
        {
            event=(struct inotify_event*)p;
            p+=sizeof(struct inotify_event)+event->len;
            if(event->mask & IN_ACCESS) printf ("%s file was accessed\n", event->name);
            if(event->mask & IN_ATTRIB) printf ("%s file attributes changed\n", event->name);
            if(event->mask & IN_CREATE) printf ("%s file created inside watched directory\n", event->name);
            if(event->mask & IN_DELETE ) printf ("%s file deleted inside watched directory\n", event->name);
            if(event->mask & IN_DELETE_SELF ) printf ("watched file %s  deleted\n", event->name);
            if(event->mask & IN_MODIFY ) printf ("%s file was modified\n", event->name);
            if(event->mask & IN_MOVE_SELF ) printf ("%s file  was moved\n", event->name);



        }


    }
}




int main(int argc, char const *argv[])
{
  

    return 0;
}
