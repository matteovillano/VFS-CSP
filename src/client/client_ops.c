#include "common.h"
#include "client.h"

extern char server_ip[];
extern int sockfd;
extern char input_buffer[];
extern int input_len;

/*
 * Handles client commands: download, upload, exit
 */
int op_command(char *command) {
    
    if (strncmp(command, "download ",9)==0){
        op_download(command);
        return -1;
    }
    if (strncmp(command, "upload ",7)==0){
        op_upload(command);
        return -1;
    }
    if (strcmp(command, "exit")==0){
        close(sockfd);
        exit(0);
    }
    
    return 0;
    
}   

/*
 * download file from the server.
 * Supports background execution via -b using fork().
 * Protocol:
 * - Sends command to server.
 * - Waits for "ready-port-<port>" message
 * - Forks if -b is set.
 * - Connects to data port and receives file content.
 */
int op_download(char *command) {
    
    char cmd_copy[BUFFER_SIZE];
    strncpy(cmd_copy, command, BUFFER_SIZE);
    cmd_copy[BUFFER_SIZE - 1] = '\0';
    
    // Parse command line arguments
    char *args[10];
    int arg_count = 0;
    char *token = strtok(cmd_copy, " \n");
    while (token && arg_count < 10) {
        args[arg_count++] = token;
        token = strtok(NULL, " \n");
    }
    
    int background = 0; // background variable
    char *server_path = NULL;
    char *client_path = NULL;

    // Check for -b flag
    if (arg_count >= 2 && strcmp(args[1], "-b") == 0) {
        background = 1;
        if (arg_count < 4) {
            printf("Usage: download -b <server_path> <client_path>\n");
            return -1;
        }
        server_path = args[2];
        client_path = args[3];
    } else {
        if (arg_count < 3) {
            printf("Usage: download <server_path> <client_path>\n");
            return -1;
        }
        server_path = args[1];
        client_path = args[2];
    }

    char *local_path = client_path;

    // Open local file for writing
    int fd = open(local_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("open failed");
        if (background) exit(1);
        return -1;
    }
    
    send(sockfd, input_buffer, input_len + 1, 0);

    // Wait for server to send port
    char buffer[BUFFER_SIZE];
    int n = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);
    if (n <= 0) {
        printf("Error: Server disconnected or error receiving port\n");
        return -1;
    }
    buffer[n] = '\0';
    
    if (strncmp(buffer, "ready-port-", 11) != 0) {
        printf("Server: %s\n", buffer);
        return -1;
    }
    
    int port = atoi(buffer + 11);
    
    // Fork if background
    if (background) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed");
            return -1;
        } else if (pid > 0) {
            // Parent
            return 0;
        }
        // Child continues below
    }

    // Connect to data port
    int data_sock = create_client_socket(server_ip, port);
    if (data_sock < 0) {
        printf("Error: Could not connect to data port %d\n", port);
        if (background) exit(1);
        return -1;
    }

    printf("Client: Downloading file to %s from server port %d...\n", local_path, port);
    
    // Transfer data
    char file_buf[BUFFER_SIZE];
    while ((n = recv(data_sock, file_buf, sizeof(file_buf), 0)) > 0) {
        if (write(fd, file_buf, n) != n) {
            perror("write failed");
            break;
        }
    }
    
    close(fd);
    close(data_sock);
    
    if (background) {
        printf("[Background] Command: download %s %s concluded\n> ", server_path, client_path);
        exit(0);
    }else {
        printf("Download successful.\n");
    }
    
    return 0;
}



/*
 * upload file to the server.
 * Supports background execution via -b using fork().
 * Protocol:
 * - Checks local file existence.
 * - Waits for "ready-port-<port>" message.
 * - Forks if -b is set.
 * - Connects to data port and sends file content.
 */
int op_upload(char *command) {
    char cmd_copy[BUFFER_SIZE];
    strncpy(cmd_copy, command, BUFFER_SIZE);
    cmd_copy[BUFFER_SIZE - 1] = '\0';
    
    // Parse command line arguments
    char *args[10];
    int arg_count = 0;
    char *token = strtok(cmd_copy, " \n");
    while (token && arg_count < 10) {
        args[arg_count++] = token;
        token = strtok(NULL, " \n");
    }
    
    int background = 0;
    char *local_path = NULL;

    // Check for -b flag
    if (arg_count >= 2 && strcmp(args[1], "-b") == 0) {
        background = 1;
        if (arg_count < 4) {
            printf("Usage: upload -b <local_path> <remote_path>\n");
            return -1;
        }
        local_path = args[2];
    } else {
        if (arg_count < 3) {
            printf("Usage: upload <local_path> <remote_path>\n");
            return -1;
        }
        local_path = args[1];
    }
    
    // Open local file for reading
    int fd = open(local_path, O_RDONLY);
    if (fd == -1) {
        printf("Error: Cannot open local file %s\n", local_path);
        return -1; 
    }

    send(sockfd, input_buffer, input_len + 1, 0);

    // Wait for server to send port
    char buffer[BUFFER_SIZE];
    int n = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);
    if (n <= 0) {
        printf("Error: Server disconnected or error receiving port\n");
        close(fd);
        return -1;
    }
    buffer[n] = '\0';
    
    if (strncmp(buffer, "ready-port-", 11) != 0) {
        printf("Server: %s\n", buffer);
        close(fd);
        return -1;
    }
    
    int port = atoi(buffer + 11);
    
    if (background) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed");
            close(fd);
            return -1;
        } else if (pid > 0) {
            // Parent
            close(fd);
            return 0;
        }
        // Child continues
    }

    // Connect to data port
    int data_sock = create_client_socket(server_ip, port);
    if (data_sock < 0) {
        printf("Error: Could not connect to data port %d\n", port);
        close(fd);
        if (background) exit(1);
        return -1;
    }
    
    printf("Client: Uploading file %s to server port %d...\n> ", local_path, port);
    
    // Transfer data
    char file_buf[BUFFER_SIZE];
    while ((n = read(fd, file_buf, sizeof(file_buf))) > 0) {
        if (send(data_sock, file_buf, n, 0) != n) {
            perror("send failed");
            break;
        }
    }
    
    close(fd);
    close(data_sock);
    
    if (background) {
        exit(0);
    }
    
    return 0;
}
