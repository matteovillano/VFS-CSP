#include "server.h"
#include "common.h"
#include "users.h"
#include <signal.h>
#include <sys/wait.h>

int root_dir_fd; // Added semicolon
char *ip;
int port;
ClientSession sessions[MAX_CLIENTS];

char *root_dir_path; // Global variable


void cleanup_children(int sig) {
    (void)sig; // unused
    restore_privileges(); // Regain root to kill any user's process
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (sessions[i].pid > 0) {
            if (kill(sessions[i].pid, SIGKILL) == 0) {
                printf("[Server] Killed child process %d\n", sessions[i].pid);
            } else {
                perror("[Server] Failed to kill child process");
            }
        }
    }
    exit(0);
}

int main(int argc, char *argv[]) {
    
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <root_dir> [ip] [port]\n", argv[0]);
        exit(EXIT_FAILURE);
    } else if (argc > 4) {
        fprintf(stderr, "too many arguments!!\n");
        exit(EXIT_FAILURE);
    }

    ip = DEFAULT_IP;
    port = DEFAULT_PORT;
    root_dir_path = argv[1];
    if (argc > 2){
        ip= argv[2];
        if (inet_pton(AF_INET, ip, NULL) != 1) {
            fprintf(stderr, "Invalid IP address\n");
            exit(EXIT_FAILURE);
        }
    }   
    if (argc > 3){
        port= atoi(argv[3]);
        if (port < 1024 || port > 65535) {
            fprintf(stderr, "Invalid port number\n");
            exit(EXIT_FAILURE);
        }
    }
    root_dir_fd = open_root_dir(root_dir_path);
    if (root_dir_fd == -1) {
        fprintf(stderr, "Invalid root directory\n");
        exit(EXIT_FAILURE);
    }

    retrive_users();
    
    init_privileges();
    minimize_privileges();


    int server_socket = create_server_socket(port); // Removed atoi as port is already int
    if (server_socket == -1) {
        exit(EXIT_FAILURE);
    }
    printf("Server started on %s:%d\n", ip, port);


    fd_set readfds;
    int max_fd;
    int i;
    
    // Initialize sessions
    for (i = 0; i < MAX_CLIENTS; i++) {
        sessions[i].pid = -1;
        sessions[i].pipe_fd_read = -1;
        sessions[i].pipe_fd_write = -1;
    }


    signal(SIGINT, cleanup_children);
    signal(SIGTERM, cleanup_children);

    while(1){
        FD_ZERO(&readfds);
        FD_SET(server_socket, &readfds);
        FD_SET(STDIN_FILENO, &readfds);
        
        max_fd = server_socket > STDIN_FILENO ? server_socket : STDIN_FILENO;
        
        // Add session pipes to readfds
        for (i = 0; i < MAX_CLIENTS; i++) {
            if (sessions[i].pid != -1) {
                FD_SET(sessions[i].pipe_fd_read, &readfds);
                if (sessions[i].pipe_fd_read > max_fd) {
                    max_fd = sessions[i].pipe_fd_read;
                }
            }
        }

        if (select(max_fd + 1, &readfds, NULL, NULL, NULL) < 0){
            if (errno == EINTR) continue; // Handle interrupted syscall
            perror("select");
            exit(EXIT_FAILURE);
        }

        // Handle new connections
        if (FD_ISSET(server_socket, &readfds)){
            
            handle_client(server_socket);
        }

        // Handle stdin
        if (FD_ISSET(STDIN_FILENO, &readfds)){
            // handle_input();
             char buf[BUFFER_SIZE];
             read(STDIN_FILENO, buf, sizeof(buf));
             if (strcmp(buf, "exit\n") == 0){
                 cleanup_children(0);
                 exit(0);
             }
        }
        
        // Handle session pipes
        for (i = 0; i < MAX_CLIENTS; i++) {
            if (sessions[i].pid != -1 && FD_ISSET(sessions[i].pipe_fd_read, &readfds)) {
                char msg[1024];
                int bytes_read = read(sessions[i].pipe_fd_read, msg, sizeof(msg) - 1);
                if (bytes_read > 0) {
                    msg[bytes_read] = '\0';
                    printf("[Parent] Received from child (PID %d): %s", sessions[i].pid, msg);
                    printf("[PARENT] session[i].pipe_fd_write: %d\n",sessions[i].pipe_fd_write);
                    write(sessions[i].pipe_fd_write, "hello\n", strlen("hello\n")+1);
                } else {
                    // Start of EOF or error - child likely closed/died
                    if (bytes_read == 0) {
                        printf("Child %d disconnected (pipe closed)\n", sessions[i].pid);
                    } else {
                        perror("read pipe");
                    }
                    close(sessions[i].pipe_fd_read);
                    close(sessions[i].pipe_fd_write);
                    sessions[i].pid = -1; // Free slot
                    sessions[i].pipe_fd_read = -1;
                }
            }
        }
    }



    return 0;
}