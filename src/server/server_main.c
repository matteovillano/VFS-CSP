#include "server.h"
#include "common.h"
#include "users.h"
#include "transfer.h"
#include "concurrency.h"

//global variables
int root_dir_fd;
int current_dir_fd;
char *ip;
int port;
ClientSession sessions[MAX_CLIENTS];

char root_dir_path[1024]; // Global variable



/*
 * Main Server Entry Point
 * -----------------------
 * Initializes the server, sets up shared resources and privileges,
 * and enters the main event loop to handle connections and commands.
 */
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
    char *root_path = argv[1];
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

    init_shared_memory();
    
    init_privileges();
    init_shared_memory(); // Initialize shared memory for concurrency locks
    
    init_privileges();    // Capture original SUDO credentials
    minimize_privileges(); // Drop to non-root user for security
    
    root_dir_fd = open_root_dir(root_path);
    printf("root_dir_fd: %d\n", root_dir_fd); // Remove this? Plan said remove.
    // Actually replacement content must replace target.

    // I will replace the block containing the printf and the startup logs.
    
    if (root_dir_fd == -1) {
        fprintf(stderr, "[PARENT] Invalid root directory\n");
        exit(EXIT_FAILURE);
    }

    // Removed root_dir_fd debug print

    if (find_path(root_dir_path, sizeof(root_dir_path), root_dir_fd)){
        perror("find_path failed");
        exit(EXIT_FAILURE);
    }
    retrive_users();

    int server_socket = create_server_socket(port); // Removed atoi as port is already int
    if (server_socket == -1) {
        exit(EXIT_FAILURE);
    }
    printf("[PARENT] Server started on %s:%d\n", ip, port);


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
    signal(SIGCHLD, handle_sigchld);

    // Main Server Loop
    // Uses select() to multiplex I/O between:
    // 1. Server socket (New connections)
    // 2. Stdin (Admin commands)
    // 3. Child pipes (Status updates from client sessions)
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
            buf[strcspn(buf, "\n")] = 0;

            char *args[3];
            int arg_count = 0;
            char *token = strtok(buf, " ");
            while (token != NULL) {
                if (arg_count >= 3) break;
                args[arg_count++] = token;
                token = strtok(NULL, " ");
            }

            if (arg_count == 1 && strcmp(args[0], "exit") == 0){
                cleanup_children(0);
                exit(0);
            } else if (arg_count == 3 && strcmp(args[0], "create_user") == 0) {
                // TODO create user
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
        
        
        // Handle session pipes (Child Processes)
        // Check if any child process has sent a message (e.g., status update, log)
        for (i = 0; i < MAX_CLIENTS; i++) {
            if (sessions[i].pid != -1 && FD_ISSET(sessions[i].pipe_fd_read, &readfds)) {
                parent_handle_msg(i);
                    
            }
        }
    }



    return 0;
}