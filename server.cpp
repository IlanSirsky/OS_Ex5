/*
** server.c -- a stream socket server demo
** Beej multi process server with stack implementation
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/mman.h>
#include "StackOverLinkedList.h"

#define PORT "3490" // the port users will be connecting to

#define BACKLOG 10 // how many pending connections queue will hold

int fd;
struct flock lock;

void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}


void *myProccessFun(void *vargp)
{
    int *hold = (int *)vargp;
    int new_fd = *hold;

    char data[1024];
    bzero(data, 1024);
    int Bytes;

    while (1)
    {
        if((Bytes = recv(new_fd, data, 1024, 0)) == -1)
        {
            perror("DEBUG: recv");
            exit(1);
        } 

        data[Bytes] = '\0';
        if (strncmp(data, "EXIT", 4) == 0){
            printf("DEBUG: Closed connection with fd: %d\n", new_fd);
            break;
        }

        lock.l_type = F_WRLCK;
        fcntl(fd, F_SETLKW, &lock);

        if (strncmp(data, "PUSH", 4) == 0)
        {
            char *pointerData = data + 5;
            push(pointerData);
            printf("DEBUG: Pushed -> %s", pointerData);
        }
        else if (strncmp(data, "POP", 3) == 0)
        {
            char *pointerData = pop();
            if(pointerData == NULL){
                printf("ERROR: Stack Underflow\n");
                lock.l_type = F_UNLCK;
                fcntl (fd, F_SETLKW, &lock);
                continue;
            }
            printf("DEBUG: Popped -> %s", pointerData);
        }
        else if (strncmp(data, "TOP",3) == 0)
        {
            char *pointerData = top();

            if(pointerData == NULL){
                printf("ERROR: Stack Underflow\n");
                lock.l_type = F_UNLCK;
                fcntl (fd, F_SETLKW, &lock);
                continue;
            }

            char out[1024];
            bzero(out, 1024);
            strcat(out, "OUTPUT: ");
            strcat(out, pointerData);
            printf("%s", out);
            if (send(new_fd, out, strlen(out), 0) == -1){
                perror("send");
            }
        }

        lock.l_type = F_UNLCK;
        fcntl (fd, F_SETLKW, &lock);
    }

    close(new_fd);
    close(fd);
    return NULL;
}

int main(void)
{
    fd = open("test.txt", O_WRONLY);
    if (fd == -1)
    {
        perror("DEBUG: Test file creation");
    }
    memset(&lock, 0, sizeof(lock));

    Node *stack = (Node *)mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (stack == MAP_FAILED)
    {
        printf("DEBUG: Mapping Failed\n");
        return 1;
    }
    int sockfd, new_fd; // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes = 1;
    char s[INET6_ADDRSTRLEN];
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "DEBUG: getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1)
        {
            perror("DEBUG: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                       sizeof(int)) == -1)
        {
            perror("DEBUG: setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)
    {
        fprintf(stderr, "DEBUG: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1)
    {
        perror("DEBUG: listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        perror("DEBUG: sigaction");
        exit(1);
    }

    printf("DEBUG: waiting for connections...\n");
    int counter = 0;
    while(1) {  // main accept() loop
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s);
        printf("server: got connection from %s\n", s);

        if (!fork()) { // this is the child process
            close(sockfd); // child doesn't need the listener
            counter++;
            myProccessFun(&new_fd);
            close(new_fd);
            exit(0);
        }
        close(new_fd);  // parent doesn't need this
    }

    return 0;
}