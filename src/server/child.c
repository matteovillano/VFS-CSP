#include "server.h"
#include "common.h"
#include "users.h"
#include "transfer.h"
#include <signal.h>

extern ClientSession sessions[MAX_CLIENTS];
int sockfd;
int pipe_read;
int pipe_write;

/*
 * Manages a new client connection.
 * 1. Accepts the connection.
 * 2. Finds a free session slot.
 * 3. Creates pipes for Parent<->Child.
 * 4. Forks a child process to handle the session.
 * 5. Parent tracks the child pid/pipes.
 */
int handle_client(int server_socket) {
    // Accept connection
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
    if (client_socket < 0) {
        perror("Accept failed");
        return -1;
    }
    
    printf("[PARENT] New connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    // Find free slot
    int slot = -1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (sessions[i].pid == -1) {
            slot = i;
            break;
        }
    }
    if (slot == -1) {
        printf("Max clients reached, connection rejected (for now)\n");
        close(client_socket);
        return -1;
    }

    // Create pipes
    int p_to_c[2]; // Parent to Child
    int c_to_p[2]; // Child to Parent
    if (pipe(p_to_c) == -1 || pipe(c_to_p) == -1) {
        perror("pipe");
        close(client_socket);
        return -1;
    }

    // Fork child process
    pid_t pid = fork();
    if (pid < 0) {
        // Fork failed
        perror("Fork failed");
        close(client_socket);
        close(p_to_c[0]); 
        close(p_to_c[1]);
        close(c_to_p[0]); 
        close(c_to_p[1]);
        return -1;
    } else if (pid == 0) {
        // Child process
        close(server_socket); // Close listening socket

        // Setup signal handlers to default behavior
        signal(SIGINT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);

        // Setup pipes for child
        close(p_to_c[1]); // Close write end of parent-to-child
        close(c_to_p[0]); // Close read end of child-to-parent
        
        // Setup pipe file descriptors
        pipe_read = p_to_c[0];
        pipe_write = c_to_p[1];

        // If parent dies, send SIGKILL to this child
        prctl(PR_SET_PDEATHSIG, SIGKILL);
        if (getppid() == 1) {
            _exit(0); 
        }

        sockfd = client_socket; 
        
        retrive_users();
        handle_user();
        exit(0);
    } else {
        // Parent process
        close(client_socket); // Close client socket (child handles it)
        
        // Setup pipes for parent
        close(p_to_c[0]); // Close read end of parent-to-child
        close(c_to_p[1]); // Close write end of child-to-parent

        sessions[slot].pid = pid;
        sessions[slot].pipe_fd_read = c_to_p[0];  // Read from child
        sessions[slot].pipe_fd_write = p_to_c[1]; // Write to child
        printf("[PARENT] added session %d with pid %d and pipe fd %d %d\n", slot, pid, c_to_p[0], p_to_c[1]);

        return 0; // Success
    }
}

/*
 * Main loop for the child process.
 * Uses select() to monitor:
 * 1. Client socket (User commands)
 * 2. Pipe from parent (Control messages)
 */
int handle_user(){
    char buffer[BUFFER_SIZE];
    int n; // Number of bytes read
    fd_set readfds; // File descriptors for select
    int max_fd; // Maximum file descriptor for select
    
    while (1) {
        // Setup file descriptors for select
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        // FD_SET(pipe_read, &readfds);

        max_fd = sockfd;
        // if (pipe_read > max_fd)
        //     max_fd = pipe_read;

        // Wait for data
        if (select(max_fd + 1, &readfds, NULL, NULL, NULL) < 0){
            if (errno == EINTR) continue;
            perror("select");
            exit(EXIT_FAILURE);
        }

        // Check for data on TCP socket
        if (FD_ISSET(sockfd, &readfds)) {
            memset(buffer, 0, sizeof(buffer));
            // Read from TCP socket (from user)
            n = recv(sockfd, buffer, sizeof(buffer), 0);
            if (n < 0) {
                perror("Recv failed");
                break;
            }
            if (n == 0) {
                printf("[PID: %d] Client disconnected\n", getpid());
                break;
            }

            // Remove trailing newline
            buffer[strcspn(buffer, "\n")] = 0;
        
            execute_command(buffer);
        }
    }
    close(sockfd);
    // close(pipe_read);
    // close(pipe_write);
    return 0;
}

/*
 * Parses and executes user commands received over the socket.
 */
int execute_command(char *command) {
    char *args[3];

    // if exit command, exit
    if (strncmp(command, "exit", 4) == 0) {
        exit(0);
    }
    
    // Tokenize input
    int arg_count = 0;
    char *token = strtok(command, " ");
    while (token != NULL) {
        if (arg_count >= 3) break;
        args[arg_count++] = token;
        token = strtok(NULL, " ");
    }

    /* it is not specified in the assignment but it can be a valid command
    if (arg_count == 3) {
        if (strcmp(args[0], "create_user") == 0) {
            if (check_permissions(args[2]) < 0) {
                send_string("err-invalid permissions\n");
            } else {
                int permissions = (int)strtol(args[2], NULL, 8);
                int user = create_user(args[1], permissions);
                if (user == -1) {
                    send_string("err-user not created\n");
                } else {
                    send_string("user created\n");
                }
            }
        } else {
            send_string("err-invalid command\n");
        }
    } else
    */
    if (arg_count == 2) {
        /* it is not specified in the assignment but it can be a valid command */
        /*if (strcmp(args[0], "delete") == 0) {
            int ret = delete_user(args[1]);
            if (ret == -1) {
                send_string("err-user not deleted\n");
            } else {
                send_string("user deleted\n");
            }
        } else*/
        if (strcmp(args[0], "login") == 0) { // login
            retrive_users();
            if (!user_exists(args[1])) { // check if user exists
                login(args[1]);
            }else{
                send_string("err-user does not exist\n");
            }
        } else {
            send_string("err-invalid command\n");
        }
    } else {
        send_string("err-invalid command\n");
    }

    return 0;
}

