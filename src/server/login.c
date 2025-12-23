#include "server.h"
#include "common.h"
#include "users.h"
#include "ops.h"
#include <sys/prctl.h>
#include <signal.h>

extern int root_dir_fd;
extern int sockfd;
extern int current_dir_fd;
char username[USERNAME_LENGTH];
extern char root_dir_path[];
char current_dir_path[1024+USERNAME_LENGTH+2];


int login(char *usern) {
    strncpy(username, usern, strlen(usern));
    username[strlen(usern)] = '\0';    

    struct passwd *pwd = getpwnam(username); // get user info
    if (pwd == NULL) {
        perror("getpwnam failed");
        return -1;
    }

    restore_privileges(); // Ensure we are root to change UID and chroot

    if (chroot(root_dir_path) != 0) {
        perror("chroot failed");
        return -1;
    }
    if (chdir("/") != 0) {
        perror("chdir / failed");
        return -1;
    }

    // Change to user's home directory (relative to new root)
    if (chdir(username) != 0) {
        perror("chdir username failed");
        return -1;
    }

    if (setgid(pwd->pw_gid) != 0) {
        perror("setgid failed");
        return -1;
    }
    if (setuid(pwd->pw_uid) != 0) {
        perror("setuid failed");
        return -1;
    }

    // Re-arm PDEATHSIG after setuid, as setuid clears it
    if (prctl(PR_SET_PDEATHSIG, SIGKILL) == -1) {
        perror("prctl failed");
        exit(-1);
    }
    // Verify parent is still alive
    if (getppid() == 1) {
        exit(0);
    }

    printf("Process identity switched to UID: %d, GID: %d\n", pwd->pw_uid, pwd->pw_gid);
    
    
   
    current_dir_fd = openat(root_dir_fd, username, O_RDONLY | O_DIRECTORY);
    if (current_dir_fd == -1) {
        perror("Failed to open user directory");
        return -1;
    }

    snprintf(current_dir_path, sizeof(current_dir_path), "%s/%s", root_dir_path, username);
    printf("current_dir_path: %s\n", current_dir_path);
    

    send_string("Login successful\n");

    while(1){
        char buffer[BUFFER_SIZE];
        memset(buffer, 0, BUFFER_SIZE);
        int n;

        n = recv(sockfd, buffer, BUFFER_SIZE, 0);
        buffer[n+1] = '\0';
        if (n < 0) {
            perror("Recv failed");
            exit(0);
        }
        if (n == 0) {
            printf("Client disconnected\n");
            exit(0);
        }
        buffer[strcspn(buffer, "\n")] = 0;
        
        char *args[10];
        int arg_count = 0;
        char *token = strtok(buffer, " ");
        while (token != NULL && arg_count < 10) {
            args[arg_count++] = token;
            token = strtok(NULL, " ");
        }

        if (arg_count == 0) continue;

        if (strcmp(args[0], "create") == 0) {
            op_create(&args[1], arg_count - 1);
        }
        else if (strcmp(args[0], "chmod") == 0) {
            op_changemod(&args[1], arg_count - 1);
        }
        else if (strcmp(args[0], "move") == 0) {
            op_move(&args[1], arg_count - 1);
        }
        else if (strcmp(args[0], "upload") == 0) {
            //send_string("i recived upload command\n");
            op_upload(&args[1], arg_count - 1);
        }
        else if (strcmp(args[0], "download") == 0) {
            //send_string("i recived download command\n");
            op_download(&args[1], arg_count - 1);
        }
        else if (strcmp(args[0], "cd") == 0) {
            op_cd(&args[1], arg_count - 1);
        }
        else if (strcmp(args[0], "list") == 0) {
            op_list(&args[1], arg_count - 1);
        }
        else if (strcmp(args[0], "read") == 0) {
            op_read(&args[1], arg_count - 1);
        }
        else if (strcmp(args[0], "write") == 0) {
            op_write(&args[1], arg_count - 1);
        }
        else if (strcmp(args[0], "delete") == 0) {
            op_delete(&args[1], arg_count - 1);
        }
        else if (strcmp(args[0], "transfer_request") == 0) {
            //send_string("i recived transfer_request command\n");
            op_transfer_request(&args[1], arg_count - 1);
        }
        else if (strcmp(args[0], "accept") == 0) {
            send_string("i recived accept command\n");
            //op_accept(&args[1], arg_count - 1);
        }
        else if (strcmp(args[0], "reject") == 0) {
            send_string("i recived reject command\n");
            //op_reject(&args[1], arg_count - 1);
        }
        else if (strcmp(args[0], "exit") == 0) {
             exit(0);
        }
        else {
            send_string("err-Invalid command\n");
        }
        
    }
}