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


#define PORT "3490" // the port users will be connecting to

#define BACKLOG 10 // how many pending connections queue will hold

// Structure to create a Stack
struct Stack
{
    char data[1024];
    int index;
};

int fd; //file descriptor for the test file
struct flock lock; //lock for multi process connections
Stack * stack; //the stack

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
        if (strncmp(data, "EXIT", 4) == 0){ //closing the connection with the client
            printf("DEBUG: Closed connection with fd: %d\n", new_fd);
            break;
        }

        lock.l_type = F_WRLCK; //locking the lock
        fcntl(fd, F_SETLKW, &lock);

        if (strncmp(data, "PUSH", 4) == 0) //pushing the data to the stack
        {
            memcpy(data, data + 5, 1024 - 5);
            strcpy(stack[stack->index++].data, data);
            printf("DEBUG: Pushed -> %s", data);
        }
        else if (strncmp(data, "POP", 3) == 0) //popping the head of the stack
        {           
            char * pop = stack[stack->index - 1].data;

            if (strncmp(pop, "NULL", 3) == 0)
            {
                printf("ERROR: Stack Underflow\n");
                lock.l_type = F_UNLCK; //unlocking the lock in case of underflow
                fcntl (fd, F_SETLKW, &lock);
                continue;
            }
            printf("DEBUG: Popped -> %s", pop);
            strcpy(stack[stack->index--].data, "");
        }
        else if (strncmp(data, "TOP",3) == 0) //showing the head of the stack
        {
            char * top = stack[stack->index - 1].data;

            if (strncmp(top, "NULL", 3) == 0)
            {
                printf("ERROR: Stack Underflow\n");
                if (send(new_fd, "ERROR: Stack Underflow\n", strlen("ERROR: Stack Underflow\n"), 0) == -1){
                    perror("send");
                }
                lock.l_type = F_UNLCK; //unlocking the lock in case of underflow
                fcntl (fd, F_SETLKW, &lock);
                continue;
            }
            char out[1024];
            bzero(out, 1024);
            strcat(out, "OUTPUT: ");
            strcat(out, top);
            printf("%s", out);
            if (send(new_fd, out, strlen(out), 0) == -1){
                perror("send");
            }
        }
        lock.l_type = F_UNLCK; //unlocking the lock
        fcntl (fd, F_SETLKW, &lock);
    }

    close(new_fd);
    close(fd);
    return NULL;
}

int main(void)
{
    fd = open("test.txt", O_WRONLY); //opening a test file
    if (fd == -1)
    {
        perror("DEBUG: Test file creation");
    }
    memset(&lock, 0, sizeof(lock)); //initializing the lock

    stack = (Stack *)mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0); //creating a shared memory stack
    if (stack == MAP_FAILED)
    {
        printf("DEBUG: Mapping Failed - stack\n");
        return 1;
    }
    strcpy(stack[stack->index++].data, "NULL");
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
            myProccessFun(&new_fd); // call the function to handle the connection
            close(new_fd);
            exit(0);
        }
        close(new_fd);  // parent doesn't need this
    }

    return 0;
}