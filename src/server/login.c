#include "server.h"
#include "common.h"
#include "users.h"
#include "ops.h"
#include "transfer.h"
#include <sys/prctl.h>
#include <signal.h>

extern int root_dir_fd;
extern int sockfd;
extern int current_dir_fd;
extern int pipe_read;
extern char root_dir_path[];
char username[USERNAME_LENGTH];
char current_dir_path[1024+USERNAME_LENGTH+2];


int login(char *usern) {
    strncpy(username, usern, strlen(usern));
    username[strlen(usern)] = '\0';    

    struct passwd *pwd = getpwnam(username); // get user info of username
    if (pwd == NULL) {
        perror("getpwnam failed");
        return -1;
    }

    restore_privileges(); // Ensure we are root to change UID and chroot

    // Change root directory to user's home directory
    if (chroot(root_dir_path) != 0) {
        perror("chroot failed");
        return -1;
    }

    // Change working directory to root
    if (chdir("/") != 0) {
        perror("chdir / failed");
        return -1;
    }

    // Change to user's home directory
    if (chdir(username) != 0) {
        perror("chdir username failed");
        return -1;
    }

    // Set group ID and user ID to user's ID
    if (setgid(pwd->pw_gid) != 0) {
        perror("setgid failed");
        return -1;
    }
    if (setuid(pwd->pw_uid) != 0) {
        perror("setuid failed");
        return -1;
    }

    // Re-set PDEATHSIG after setuid, as setuid clears it
    if (prctl(PR_SET_PDEATHSIG, SIGKILL) == -1) {
        perror("prctl failed");
        exit(-1);
    }
    // Verify parent is still alive
    if (getppid() == 1) {
        exit(0);
    }

    printf("Process identity switched to UID: %d, GID: %d\n", pwd->pw_uid, pwd->pw_gid);
    
    // Open user's home directory
    current_dir_fd = openat(root_dir_fd, username, O_RDONLY | O_DIRECTORY);
    if (current_dir_fd == -1) {
        perror("Failed to open user directory");
        return -1;
    }

    snprintf(current_dir_path, sizeof(current_dir_path), "%s/%s", root_dir_path, username);
    printf("current_dir_path: %s\n", current_dir_path);
    
    send_string("Login successful\n");

    i_am_user(); // to handle transfer_requests

    fd_set readfds;
    int max_fd;

    while(1){
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        FD_SET(pipe_read, &readfds);

        max_fd = (sockfd > pipe_read ? sockfd : pipe_read);

        if (select(max_fd + 1, &readfds, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) continue;
            perror("select");
            exit(EXIT_FAILURE);
        }

        // Check for message from parent
        if (FD_ISSET(pipe_read, &readfds)) {
            child_handle_msg();
        }

        // Check for message from client
        if (FD_ISSET(sockfd, &readfds)) {
            char buffer[BUFFER_SIZE];
            memset(buffer, 0, BUFFER_SIZE);
            int n;

            // Receive message from client
            n = recv(sockfd, buffer, BUFFER_SIZE, 0);
            if (n < 0) {
                perror("Recv failed");
                exit(0);
            }
            if (n == 0) {
                printf("Client disconnected\n");
                exit(0);
            }

            // Ensure null termination
            if (n < BUFFER_SIZE) 
                buffer[n] = '\0';
            else 
                buffer[BUFFER_SIZE - 1] = '\0';

            buffer[strcspn(buffer, "\n")] = 0;
            
            // Parse command
            char *args[10];
            int arg_count = 0;
            char *token = strtok(buffer, " ");
            while (token != NULL && arg_count < 10) {
                args[arg_count++] = token;
                token = strtok(NULL, " ");
            }

            if (arg_count-- == 0) continue;

            // Handle command
            if (strcmp(args[0], "create") == 0) {
                op_create(&args[1], arg_count);
            }
            else if (strcmp(args[0], "chmod") == 0) {
                op_changemod(&args[1], arg_count);
            }
            else if (strcmp(args[0], "move") == 0) {
                op_move(&args[1], arg_count);
            }
            else if (strcmp(args[0], "upload") == 0) {
                op_upload(&args[1], arg_count);
            }
            else if (strcmp(args[0], "download") == 0) {
                op_download(&args[1], arg_count);
            }
            else if (strcmp(args[0], "cd") == 0) {
                op_cd(&args[1], arg_count);
            }
            else if (strcmp(args[0], "list") == 0) {
                op_list(&args[1], arg_count);
            }
            else if (strcmp(args[0], "read") == 0) {
                op_read(&args[1], arg_count);
            }
            else if (strcmp(args[0], "write") == 0) {
                op_write(&args[1], arg_count);
            }
            else if (strcmp(args[0], "delete") == 0) {
                op_delete(&args[1], arg_count);
            }
            else if (strcmp(args[0], "transfer_request") == 0) {
                op_transfer_request(&args[1], arg_count);
            }
            else if (strcmp(args[0], "accept") == 0) {
                op_accept(&args[1], arg_count);
            }
            else if (strcmp(args[0], "reject") == 0) {
                op_reject(&args[1], arg_count);
            }
            else if (strcmp(args[0], "exit") == 0) {
                exit(0);
            }
            else {
                send_string("err-Invalid command\n");
            }
        }
    }
}