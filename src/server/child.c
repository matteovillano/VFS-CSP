#include "server.h"
#include "common.h"
#include "users.h"
#include "transfer.h"
#include <signal.h>

int sockfd;
extern ClientSession sessions[MAX_CLIENTS];
int pipe_read;
int pipe_write;


int handle_client(int server_socket) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);

    if (client_socket < 0) {
        perror("Accept failed");
        return -1;
    }

    printf("New connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

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

    //ClientSession *session = &sessions[slot];

    int p_to_c[2]; // Parent to Child
    int c_to_p[2]; // Child to Parent

    if (pipe(p_to_c) == -1 || pipe(c_to_p) == -1) {
        perror("pipe");
        close(client_socket);
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
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

        signal(SIGINT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);

        // Setup pipes for child
        close(p_to_c[1]); // Close write end of parent-to-child
        close(c_to_p[0]); // Close read end of child-to-parent
        
        pipe_read = p_to_c[0];
        pipe_write = c_to_p[1];

        // If parent dies, send SIGKILL to this child
        prctl(PR_SET_PDEATHSIG, SIGKILL);
        if (getppid() == 1) {
            _exit(0); 
        }

        // We might still need client_socket to communicate with the actual client (TCP)
        // For now, the prompt says "generate a child process it also create 2 pipes to allow interprocess comunication"
        // It implies the child handles the client (socket) AND talks to parent (pipe).
        // Using global sockfd as per previous code, but updating to use local or passed var is better.
        sockfd = client_socket; 
        
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



int handle_user(){
    char buffer[BUFFER_SIZE];
    int n;
    


    printf("Handling client (PID: %d)\n", getpid());

    fd_set readfds;
    int max_fd;
    
    while (1) {
        

        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        FD_SET(pipe_read, &readfds);

        max_fd = sockfd;
        if (pipe_read > max_fd)
            max_fd = pipe_read;

        if (select(max_fd + 1, &readfds, NULL, NULL, NULL) < 0){
            if (errno == EINTR) continue;
            perror("select");
            exit(EXIT_FAILURE);
        }

        if (FD_ISSET(sockfd, &readfds)) {
            memset(buffer, 0, sizeof(buffer));
            // Read from TCP socket (from user)
            n = recv(sockfd, buffer, sizeof(buffer), 0);
            if (n < 0) {
                perror("Recv failed");
                break;
            }
            if (n == 0) {
                printf("Client disconnected\n");
                break;
            }

            // Remove trailing newline
            buffer[strcspn(buffer, "\n")] = 0;
        
            execute_command(buffer);
        }
        if (FD_ISSET(pipe_read, &readfds)) {
            printf("[CHILD] There is something to read in pipe\n");
            ;
            //child_handle_msg();
            
            // Send message to parent
            //char msg_to_parent[1100];
            //snprintf(msg_to_parent, sizeof(msg_to_parent), "User sent: %s\n", buffer);
            //write(pipe_write, msg_to_parent, strlen(msg_to_parent));
        }
        
        
        
        
    }
    close(sockfd);
    close(pipe_read);
    close(pipe_write);
    return 0;
}

int execute_command(char *command) {
   
    char *args[3];

    
    if (strcmp(command, "exit") == 0) {
        exit(0);
    }
    

    // Tokenize input, args[0] = command, args[1] = username, args[2] = permission
    int arg_count = 0;
    char *token = strtok(command, " ");
    while (token != NULL) {
        if (arg_count >= 3) break;
        args[arg_count++] = token;
        token = strtok(NULL, " ");
    }

    // if there are too many arguments, skip
    if (token != NULL) {
        send_string("err-invalid command\n");
        return 0;
    }

    if (arg_count == 3) {
        if (strcmp(args[0], "create_user") == 0) {
            // TODO create user
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
    } else if (arg_count == 2) {
        /* it is not specified in the assignment but it is a valid command */
        if (strcmp(args[0], "delete") == 0) {
            // TODO delete user
            int ret = delete_user(args[1]);
            if (ret == -1) {
                send_string("err-user not deleted\n");
            } else {
                send_string("user deleted\n");
            }
        } else if (strcmp(args[0], "login") == 0) {
            // TODO login
            if (!user_exists(args[1])) {
                
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

