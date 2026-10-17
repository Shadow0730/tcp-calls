#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>

#define PORT 8081
#define MAX_CLIENTS 4096
unsigned long long factorial(unsigned long long n) {
    if (n>20) {
        n=20;
    }
    unsigned long long result = 1;
    for (unsigned long long i = 2; i <= n; i++) {
        result *= i;
    }
    return result;
}
int nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags<0) {
        return -1;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}
int main(void){
    signal(SIGPIPE, SIG_IGN);

    int s_fd=socket(AF_INET,SOCK_STREAM,0);
    if (s_fd<0){
        perror("socket");
        return 1;
    }
    int r=1;
    if (setsockopt(s_fd,SOL_SOCKET, SO_REUSEADDR, &r, sizeof(r)) < 0){
        perror("setsockopt");
        close(s_fd);
        return 1;
    }
    struct sockaddr_in address = {0};
    memset(&address, 0, sizeof(address));
    address.sin_family=AF_INET;
    address.sin_addr.s_addr=htonl(INADDR_ANY);
    address.sin_port=htons(PORT);

    if (bind(s_fd, (struct sockaddr *)&address, sizeof(address))<0){
        perror("bind");
        close(s_fd);
        return 1;
    }
    if (listen(s_fd, 1)<0){
        perror("listen");
        close(s_fd);
        return 1;

    }
    nonblocking(s_fd);
    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        perror("epoll_create1");
        return 1;
    }
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = s_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, s_fd, &ev) < 0) {
        perror("epoll_ctl");
        return 1;
    }
    struct epoll_event events[MAX_CLIENTS];
    printf("server listening on port %d\n", PORT);
    fflush(stdout);
    while(1){

        int ready=epoll_wait(epoll_fd, events, MAX_CLIENTS, -1);
        if(ready<0){
            if(errno==EINTR){
                continue;
            }
            perror("epoll_wait");
            break;
        }
        for(int i=0; i<ready; i++){
            int c_fd = events[i].data.fd;
            if (c_fd == s_fd) {
                while(1){
                    int new_fd=accept(s_fd, NULL, NULL);
                    if (new_fd<0) {
                        if (errno==EAGAIN || errno==EWOULDBLOCK) {
                            break; // No more connections to accept
                        } else {
                            perror("accept");
                            break;
                        }
                    }
                    nonblocking(new_fd);
                    ev.events = EPOLLIN;
                    ev.data.fd = new_fd;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_fd, &ev) < 0) {
                        perror("epoll_ctl");
                        close(new_fd);
                    }
                }
            } else {
                char message[128]={0};
                ssize_t recieved=recv(c_fd,message,sizeof(message)-1,0);
                if (recieved<=0){
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, c_fd, NULL);
                    close(c_fd);
                    continue;
                }
                unsigned long long n = strtoul(message, NULL, 10);
                unsigned long long fact = factorial(n);
                char reply[128];
                int reply_length = snprintf(reply, sizeof(reply), "%llu", fact)+1;
                send(c_fd, reply, reply_length, 0)
            }
        }
    }
    close(epoll_fd);
    close(s_fd);
    return 0;
}