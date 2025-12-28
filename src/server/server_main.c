#include "server.h"
#include "common.h"
#include "users.h"
#include "transfer.h"
#include "concurrency.h"

//global variables
int root_dir_fd;
int current_dir_fd;
int port;
char *ip;
char root_dir_path[1024];
ClientSession sessions[MAX_CLIENTS];

int main(int argc, char *argv[]) {
    
    // arguments check
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <root_dir> <ip> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    } else if (argc > 4) {
        fprintf(stderr, "too many arguments!!\n");
        exit(EXIT_FAILURE);
    }

    /* removed default values
    ip = DEFAULT_IP;
    port = DEFAULT_PORT;
    */

    // arguments parsing
    char *root_path = argv[1];
    if (argc > 2){
        ip = argv[2];
        struct in_addr addr;
        if (inet_pton(AF_INET, ip, &addr) != 1) { // check if ip is valid, return 0 if not
            fprintf(stderr, "Invalid IP address\n");
            exit(EXIT_FAILURE);
        }
    }   
    if (argc > 3){
        port = atoi(argv[3]);
        if (port < 1024 || port > 65535) {
            fprintf(stderr, "Invalid port number\n");
            exit(EXIT_FAILURE);
        }
    }

    init_shared_memory(); // Initialize shared memory for concurrency locks
    init_privileges();    // Capture original SUDO credentials
    minimize_privileges(); // Drop to non-root user for security
    
    // create and open root directory
    root_dir_fd = open_root_dir(root_path);
    if (root_dir_fd == -1) {
        fprintf(stderr, "[PARENT] Invalid root directory\n");
        exit(EXIT_FAILURE);
    }

    // save root directory path in global variable root_dir_path
    if (find_path(root_dir_path, sizeof(root_dir_path), root_dir_fd)){
        perror("find_path failed");
        exit(EXIT_FAILURE);
    }

    // retrive users from users file
    retrive_users();

    // create server socket
    int server_socket = create_server_socket(port);
    if (server_socket == -1) {
        exit(EXIT_FAILURE);
    }
    printf("[PARENT] Server started on %s:%d\n", ip, port);

    // select variables
    fd_set readfds; // collection of file descriptors
    int max_fd; // maximum file descriptor
    int i; // loop variable
    
    // Initialize sessions for pipes
    for (i = 0; i < MAX_CLIENTS; i++) {
        sessions[i].pid = -1;
        sessions[i].pipe_fd_read = -1;
        sessions[i].pipe_fd_write = -1;
    }

    // Initialize signal handlers
    signal(SIGINT, cleanup_children); // Ctrl^C
    signal(SIGTERM, cleanup_children); // kill
    signal(SIGCHLD, handle_sigchld); // Child termination

    /*
     * Main Server Loop, handles:
     * - New connections
     * - Admin commands
     * - Child processes
     * Uses select() to multiplex I/O between:
     * - Server socket (New connections)
     * - Stdin (Admin commands)
     * - Child pipes
     */
    while(1){
        // Clear and set file descriptors
        FD_ZERO(&readfds);
        FD_SET(server_socket, &readfds);
        FD_SET(STDIN_FILENO, &readfds);
        
        // Find maximum file descriptor
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

        // Wait for I/O events
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
            // Read input from stdin
            char buf[BUFFER_SIZE];
            read(STDIN_FILENO, buf, sizeof(buf));
            buf[strcspn(buf, "\n")] = 0; // Remove trailing newline

            // Parse input
            char *args[3];
            int arg_count = 0;
            char *token = strtok(buf, " ");
            while (token != NULL) {
                if (arg_count >= 3) break;
                args[arg_count++] = token;
                token = strtok(NULL, " ");
            }

            // Handle commands
            if (arg_count == 1 && strcmp(args[0], "exit") == 0){ // exit
                cleanup_children(0);
                exit(0);
            } else if (arg_count == 3 && strcmp(args[0], "create_user") == 0) { // create user
                // check if permission are valid
                if (check_permissions(args[2]) < 0) {
                    printf("err-invalid permissions\n");
                } else {
                    int permissions = (int)strtol(args[2], NULL, 8);
                    int user = create_user(args[1], permissions);
                    if (user == -1) {
                        printf("err-user not created\n");
                    } else {
                        printf("user created\n");
                    }
                }
            } else {
                printf("err-Invalid command\n");
            }
        }
        
        // Handle session pipes, check if any child process has sent a message
        for (i = 0; i < MAX_CLIENTS; i++) {
            if (sessions[i].pid != -1 && FD_ISSET(sessions[i].pipe_fd_read, &readfds)) {
                parent_handle_msg(i);
            }
        }
    }

    return 0;
}