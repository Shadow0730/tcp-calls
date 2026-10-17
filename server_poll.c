#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <poll.h>
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
    struct pollfd fds[MAX_CLIENTS + 1];
    fds[0].fd = s_fd;
    fds[0].events = POLLIN;
    int nfds = 1;
    printf("server listening");
    while(1){

        int ready=poll(fds, nfds, -1);
        if(ready<0){
            if(errno==EINTR){
                continue;
            }
            perror("poll");
            break;
        }
        if (fds[0].revents & POLLIN) {
            while(1){
                int c_fd=accept(s_fd, NULL, NULL);
                if (c_fd<0) {
                    if (errno==EAGAIN || errno==EWOULDBLOCK) {
                        break; // No more connections to accept
                    } else {
                        perror("accept");
                        break;
                    }
                }
                if(nfds>=MAX_CLIENTS) {
                    close(c_fd);
                    continue;
                }
                nonblocking(c_fd);
                fds[nfds].fd = c_fd;
                fds[nfds].events = POLLIN;
                nfds++;
            }
        }
        for(int fd=1; fd<nfds; fd++){
            if(!(fds[fd].revents & (POLLIN|POLLHUP|POLLERR))){
                continue;
            }
            char message[128];
            ssize_t recieved=recv(fds[fd].fd,message,sizeof(message)-1,0);
            if (recieved<=0){
                close(fds[fd].fd);
                fds[fd] = fds[nfds - 1]; // Replace with the last fd
                nfds--;
                fd--; // Check the new fd at this index
                continue;
            }
            unsigned long long n = strtoul(message, NULL, 10);
            unsigned long long fact = factorial(n);
            char reply[128];
            int reply_length = snprintf(reply, sizeof(reply), "%llu", fact);
            if(send(fds[fd].fd, reply, reply_length, 0) < 0){
                perror("send");
            }
        }
    }
    close(s_fd);
    return 0;
}