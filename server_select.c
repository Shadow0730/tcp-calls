#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#define PORT 8081

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
void* h_client(void* arg){
    int c_fd = (int)(intptr_t)arg;
    char message[128];
    while(1){
        ssize_t recieved=recv(c_fd,message,sizeof(message)-1,0);
        if (recieved<=0){
            break;
        }
        unsigned long long n = strtoul(message, NULL, 10);
        unsigned long long fact = factorial(n);
        char reply[128];
        int reply_length = snprintf(reply, sizeof(reply), "%llu", fact);
        if(send(c_fd, reply, reply_length, 0) < 0){
            perror("send");
            break;
        }
    }
    close(c_fd);
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
    fd_set M;
    FD_ZERO(&M);
    FD_SET(s_fd, &M);
    int max_fd=s_fd;
    printf("server listening");
    while(1){

        fd_set R=M;
        int ready=select(max_fd+1, &R, NULL, NULL, NULL);
        if(ready<0){
            if(errno==EINTR){
                continue;
            }
            perror("select");
            break;
        }
        for(int fd=0; fd<=max_fd; fd++){
            if(!FD_ISSET(fd, &R)){
                continue;
            }
            if (fd==s_fd) {
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
                    nonblocking(c_fd);
                    FD_SET(c_fd, &M);
                    if (c_fd>max_fd) {
                        max_fd=c_fd;
                    }
                }
            } else{
                char message[128];
                ssize_t recieved=recv(fd,message,sizeof(message)-1,0);
                if (recieved<=0){
                    FD_CLR(fd, &M);
                    close(fd);
                    continue;
                }
                unsigned long long n = strtoul(message, NULL, 10);
                unsigned long long fact = factorial(n);
                char reply[128];
                int reply_length = snprintf(reply, sizeof(reply), "%llu", fact);
                send(fd, reply, reply_length, 0);
            }
        }
    }
    close(s_fd);
    return 0;
}