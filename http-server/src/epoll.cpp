#include <sys/epoll.h>
#include <unistd.h>
#include <stdio.h>


int main() {

    struct epoll_event event;
    struct epoll_event events[5];
    int epoll_fd = epoll_create1(0);

    int len_rd;
    char buf[4096];

    if (epoll_fd == -1)
        return 1;
    
    event.events = EPOLLIN;
    event.data.fd = 0;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, 0, &event)) {
        printf("failed to add\n");
        return 1;
    }
  

    int i;
    int event_count = epoll_wait(epoll_fd, events, 5, 5000);

    if (!event_count) {
        printf("timeout\n");
    } else {
        for (i = 0; i < event_count; i++) {
            printf("handling file descriptor: %u\n", events[i].data.fd);
            len_rd = read(events[i].data.fd, buf, sizeof(buf));
            buf[len_rd]= '\0';
            printf("%s", buf);
        }
        
    }

    if (close(epoll_fd)) {
        printf("failed to close\n");
        return 1;
    }
    return 0;
}
