#include "ops.h"
#include "users.h"
#include "common.h"
#include "server.h"
#include "transfer.h"
#include "concurrency.h"
#include <errno.h>

#define PATH_LENGTH 1024

extern int root_dir_fd;
extern int current_dir_fd;
extern int sockfd;
extern char username[USERNAME_LENGTH];
extern char root_dir_path[];
extern char current_dir_path[];

/*
 * Creates a file or directory with specified permissions.
 */
int op_create(char *args[], int arg_count) {
    char msg[256];
    
    // Check errors in arguments
    if (arg_count < 2 || arg_count > 3) {
        send_string("err-Usage: create <filename> <permissions> or create -d <dirname> <permissions>");
        return -1;
    }
    if (arg_count == 3 && strcmp(args[0], "-d") != 0) {
        send_string("err-Usage: create <filename> <permissions> or create -d <dirname> <permissions>");
        return -1;
    }

    // Identify target path
    char *target_path = (strcmp(args[0], "-d") == 0) ? args[1] : args[0];
    
    // Validate path
    if (check_path_mine(target_path) != 0) {
        send_string("err-Invalid path");
        return -1;
    }

    // Check if the client wants to create a directory or a file
    if (strcmp(args[0], "-d") == 0) {
        if (arg_count < 3) {
            send_string("err-Usage: create -d <dirname> <permissions>");
            return -1;
        }

        // Check permissions
        if (check_permissions(args[2]) != 0){
            send_string("err-permission not valid");
            return -1;
        }
        mode_t mode = (mode_t)strtol(args[2], NULL, 8);

        mode_t old_umask = umask(0000); // removed the mask

        // Create directory
        if (mkdirat(current_dir_fd, args[1], mode) == -1) {
            send_string("err-Error creating directory");
            perror("Error creating directory");
            return -1;
        }

        umask(old_umask); // restore the mask

        snprintf(msg, sizeof(msg), "ok-Directory %s created successfully with permissions %o.", args[1], mode);
        send_string(msg);
    } else {
        if (check_permissions(args[1]) != 0){
            send_string("err-permission not valid");
            return -1;
        }
        mode_t mode = (mode_t)strtol(args[1], NULL, 8);

        mode_t old_umask = umask(0000);

        // Create file
        int fd = openat(current_dir_fd, args[0], O_CREAT | O_WRONLY | O_EXCL, mode);
        if (fd == -1) {
            send_string("err-Error creating file");
            perror("Error creating file");
            return -1;
        }

        umask(old_umask);
        
        printf("[PID: %d] File %s created successfully with permissions %o.\n", getpid(), args[0], mode);

        close(fd);

        snprintf(msg, sizeof(msg), "ok-File %s created successfully with permissions %o.", args[0], mode);
        send_string(msg);
    }
    return 0;
}

/*
 * Changes the permissions of a file or directory.
 */
int op_changemod(char *args[], int arg_count) {
    
    char msg[256];

    if (arg_count != 2) {
        send_string("err-Usage: chmod <path> <permissions>");
        return -1;
    }
    
    if( check_permissions(args[1]) != 0){
        send_string("err-permission not valid");
        return -1;
    }

    mode_t mode = (mode_t)strtol(args[1], NULL, 8);

    if (check_path_mine(args[0]) != 0) {
        send_string("err-Invalid path");
        return -1;
    }

    // Change permissions
    if (fchmodat(current_dir_fd, args[0], mode, 0) == -1) {
        perror("chmod failed");
        send_string("err-Error changing permissions");
        return -1;
    }

    snprintf(msg, sizeof(msg), "ok-Permissions for %s changed to %o.", args[0], mode);
    send_string(msg);

    return 0;
}

/*
 * Moves a file or directory to a new location.
 */
int op_move(char *args[], int arg_count) {
    char msg[256];
    if (arg_count != 2) {
        send_string("err-Usage: move <source> <destination>");
        return -1;
    }

    if (check_path_mine(args[0]) != 0) {
        send_string("err-Invalid source path");
        return -1;
    }
    if (check_path_mine(args[1]) != 0) {
        send_string("err-Invalid destination path");
        return -1;
    }
    
    // Resolve paths
    char source_path[PATH_MAX];
    char destination_path[PATH_MAX];
    resolve_path(current_dir_path, args[0], source_path);
    resolve_path(current_dir_path, args[1], destination_path);

    // Get file locks, for concurrency
    FileLock *source_lock = get_file_lock(source_path);
    if (!source_lock) {
        send_string("err-Server busy (too many locks)");
        return -1;
    }
    FileLock *destination_lock = get_file_lock(destination_path);
    if (!destination_lock) {
        send_string("err-Server busy (too many locks)");
        return -1;
    }

    // Locks for concurrency
    writer_lock(source_lock);
    writer_lock(destination_lock);

    // Move file
    if (renameat(current_dir_fd, args[0], current_dir_fd, args[1]) == -1) {
        perror("rename failed");
        send_string("err-Error moving file");
        writer_unlock(source_lock);
        release_file_lock(source_lock);
        writer_unlock(destination_lock);
        release_file_lock(destination_lock);
        return -1;
    }

    // Unlock files
    writer_unlock(source_lock);
    release_file_lock(source_lock);
    writer_unlock(destination_lock);
    release_file_lock(destination_lock);

    snprintf(msg, sizeof(msg), "ok-Moved %s to %s.", args[0], args[1]);
    send_string(msg);
    return 0;
}


/*
 * upload file from client to server.
 * Supports background operation with -b flag using fork().
 * Protocol:
 * - Server creates a socket.
 * - Server sends port to client.
 * - Client connects and transfers data.
 * Uses Writer lock for file concurrency.
 */
int op_upload(char *args[], int arg_count) {
    int background = 0;
    char *dest_path_str = NULL;
    char *client_path_str = NULL;
    
    // Check argument for -b
    if (arg_count >= 1 && strcmp(args[0], "-b") == 0) {
        background = 1; // set background flag
        if (arg_count < 3) {
            send_string("err-Usage: upload -b <client_path> <server_path>");
            return -1;
        }
        client_path_str = args[1];
        dest_path_str = args[2];
    } else {
        if (arg_count < 2) {
            send_string("err-Usage: upload <client_path> <server_path>");
            return -1;
        }
        client_path_str = args[0];
        dest_path_str = args[1];
    }

    if (check_path_mine(dest_path_str) != 0) {
        send_string("err-Invalid path");
        return -1;
    }

    char resolved[2048];
    resolve_path(current_dir_path, dest_path_str, resolved);

    // If background, create new process that handles it
    if (background) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed");
            send_string("err-Server fork failed");
            return -1;
        } else if (pid > 0) {
            // Parent
            return 0;
        }
        // Only child process continues below
    }
    
    // Create socket for data transfer
    int server_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        if (!background) send_string("err-Server error creating socket");
        else {
            send_string("err-Server error creating socket");
            exit(1); 
        }
        return -1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = 0; // any available port

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        if (!background) send_string("err-Server error binding socket");
        else {
            send_string("err-Server error binding socket");
            exit(1);
        }
        return -1;
    }

    if (listen(server_fd, 1) < 0) {
        perror("listen failed");
        close(server_fd);
        if (!background) send_string("err-Server error listening");
        else {
            send_string("err-Server error listening");
            exit(1);
        }
        return -1;
    }

    // Get the assigned port
    if (getsockname(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen) == -1) {
        perror("getsockname failed");
        close(server_fd);
        if (!background) send_string("err-Server error getting port");
        else {
            send_string("err-Server error getting port");
            exit(1);
        }
        return -1;
    }

    // Send port to client
    int port = ntohs(address.sin_port);
    char msg[64];
    snprintf(msg, sizeof(msg), "ready-port-%d", port);
    send_string(msg);
    
    // Accept connection
    int new_socket;
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        perror("accept failed");
        close(server_fd);
        if (background) exit(1);
        return -1;
    }

    // Prepare to write file
    FileLock *lock = get_file_lock(resolved);
    writer_lock(lock);

    int fd = openat(current_dir_fd, dest_path_str, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("openat failed");
        close(new_socket);
        send_string("err-Server error opening file");
        close(server_fd);
        writer_unlock(lock);
        release_file_lock(lock);
        if (background) exit(1);
        return -1;
    }

    // sleep 10 seconds to simulate the background operation
    if (background) sleep(10);
    
    // Receive and write file data
    char buffer[BUFFER_SIZE];
    int n;
    while ((n = recv(new_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        if (write(fd, buffer, n) != n) {
            perror("write failed");
            break;
        }
    }

    // Set file permissions
    if (fchmod(fd, 0777) == -1) {
        perror("fchmod failed");
    }

    // Close sockets and release locks
    close(fd);
    close(new_socket);
    close(server_fd);
    writer_unlock(lock);
    release_file_lock(lock);

    if (!background) {
        send_string("ok-Upload successful.");
        printf("[PID: %d] Upload finished: %s -> %s\n", getpid(), client_path_str, dest_path_str);
    } else {
        // send message to client that the background operation is finished
        char msg[2048];
        snprintf(msg, sizeof(msg), "[Background] Command: upload %s %s concluded\n", dest_path_str, client_path_str);
        send_string(msg);

        printf("[PID: %d] Background Upload finished: %s -> %s\n", getpid(), dest_path_str, client_path_str);

        close(sockfd);
        exit(0);
    }
    return 0;
}

/*
 * download file from server to client.
 * Supports background operation with -b flag using fork().
 * Protocol:
 * - Server creates a socket.
 * - Server sends port to client.
 * - Server sends file data.
 * Uses Reader lock for file concurrency.
 */
int op_download(char *args[], int arg_count) {
    int background = 0;
    char *server_path_str = NULL;
    
    // check argument for -b
    if (arg_count >= 1 && strcmp(args[0], "-b") == 0) {
        background = 1; // set background flag
        if (arg_count < 3) {
            send_string("err-Usage: download -b <server_path> <client_path>");
            return -1;
        }
        server_path_str = args[1];
    } else {
        if (arg_count < 2) {
            send_string("err-Usage: download <server_path> <client_path>");
            return -1;
        }
        server_path_str = args[0];
    }

    if (check_path_mine(server_path_str) != 0) {
        send_string("err-Invalid path");
        return -1;
    }

    char resolved[2048];
    resolve_path(current_dir_path, server_path_str, resolved);
    
    // if background, fork() 
    if (background) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed");
            send_string("err-Server fork failed");
            return -1;
        } else if (pid > 0) {
            // Parent
            return 0;
        }
        // Only Child process continues below
    }

    // get file lock
    FileLock *lock = get_file_lock(resolved);
    reader_lock(lock);

    // open file
    int fd = openat(current_dir_fd, server_path_str, O_RDONLY);
    if (fd == -1) {
        perror("openat failed");
        reader_unlock(lock);
        release_file_lock(lock);
        if (!background) send_string("err-File not found or unreadable");
        else {
            send_string("err-File not found or unreadable");
            exit(1);
        }
        return -1;
    }

    // Create socket for data transfer
    int server_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        close(fd);
        reader_unlock(lock);
        release_file_lock(lock);
        if (!background) send_string("err-Server error creating socket");
        else {
            send_string("err-Server error creating socket");
            exit(1);
        }
        return -1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = 0; // any available port

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        close(fd);
        reader_unlock(lock);
        release_file_lock(lock);
        if (!background) send_string("err-Server error binding socket");
        else {
            send_string("err-Server error binding socket");
            exit(1);
        }
        return -1;
    }

    if (listen(server_fd, 1) < 0) {
        perror("listen failed");
        close(server_fd);
        close(fd);
        reader_unlock(lock);
        release_file_lock(lock);
        if (!background) send_string("err-Server error listening");
        else {
            send_string("err-Server error listening");
            exit(1);
        }
        return -1;
    }

    // Get the assigned port
    if (getsockname(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen) == -1) {
        perror("getsockname failed");
        close(server_fd);
        close(fd);
        reader_unlock(lock);
        release_file_lock(lock);
        if (!background) send_string("err-Server error getting port");
        else {
            send_string("err-Server error getting port");
            exit(1);
        }
        return -1;
    }

    int port = ntohs(address.sin_port);
    char msg[64];
    snprintf(msg, sizeof(msg), "ready-port-%d", port);
    send_string(msg);
    
    if (background) {
        close(sockfd);
    }

    // Accept connection
    int new_socket;
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        perror("accept failed");
        close(server_fd);
        close(fd);
        reader_unlock(lock);
        release_file_lock(lock);
        if (background) exit(1);
        return -1;
    }

    // sleep 10 seconds to simulate the background operation
    if (background) sleep(10);
    
    char buffer[BUFFER_SIZE];
    int n;
    while ((n = read(fd, buffer, sizeof(buffer))) > 0) {
        if (send(new_socket, buffer, n, 0) != n) {
            perror("send failed");
            break;
        }
    }

    // Close sockets and release locks
    close(fd);
    close(new_socket);
    close(server_fd);
    reader_unlock(lock);
    release_file_lock(lock);

    if (!background) {
        printf("[PID: %d] Download finished: %s\n", getpid(), server_path_str);
    } else {
        printf("[PID: %d] Background Download finished: %s\n", getpid(), server_path_str);
        exit(0);
    }
    return 0;
}

/*
 * Changes the current directory to the specified path.
 */
int op_cd(char *args[], int arg_count) {

    if (arg_count != 1) {
        send_string("err-Usage: cd <path>");
        return -1;
    }
    
    if (check_path(args[0]) != 0) {
        send_string("err-Invalid path");
        return -1;
    }

    int new_fd = openat(current_dir_fd, args[0], O_RDONLY | O_DIRECTORY);
    if (new_fd == -1) {
        perror("openat failed");
        send_string("err-Error changing directory");
        return -1;
    }
    close(current_dir_fd);
    current_dir_fd = new_fd;
    
    // Update current_dir_path
    char resolved[2048];
    resolve_path(current_dir_path, args[0], resolved);
    strncpy(current_dir_path, resolved, 1281);
    current_dir_path[1281] = '\0';
    
    printf("[PID: %d] Changed directory to: %s\n", getpid(), current_dir_path);
    send_string("ok-Directory changed successfully.");
    return 0;
}

/*
 * Lists the contents of the specified directory.
 */
int op_list(char *args[], int arg_count) {
    int dir_fd;
    if (arg_count == 0) {
        // if there aren't arguments, open the current directory
        dir_fd = openat(current_dir_fd, ".", O_RDONLY | O_DIRECTORY);
        if (dir_fd == -1) {
            perror("openat failed");
            send_string("err-Error listing directory");
            return -1;
        }
    } else {
        if (check_path(args[0]) != 0) {
            send_string("err-Invalid path");
            return -1;
        }
        dir_fd = openat(current_dir_fd, args[0], O_RDONLY | O_DIRECTORY);
        if (dir_fd == -1) {
            perror("openat failed");
            send_string("err-Error listing directory");
            return -1;
        }
    }

    // open directory
    DIR *d = fdopendir(dir_fd);
    if (!d) {
        perror("fdopendir failed");
        close(dir_fd);
        send_string("err-Error listing directory");
        return -1;
    }

    struct dirent *dir;
    struct stat st;
    char buffer[1024];
    char line[512];
    
    // Initialize buffer
    buffer[0] = '\0';
    strcat(buffer, "ok-\n");

    // Read directory and store information in buffer
    while ((dir = readdir(d)) != NULL) {
        if (fstatat(dir_fd, dir->d_name, &st, 0) == -1) {
            continue;
        }
        
        snprintf(line, sizeof(line), "%s\tSize: %ld\tPerms: %o\n", dir->d_name, st.st_size, st.st_mode & 0777);
                 
        if (strlen(buffer) + strlen(line) < sizeof(buffer) - 1) {
            strcat(buffer, line);
        } else {
            send_string(buffer);
            buffer[0] = '\0';
            strcat(buffer, line); 
        }
    }
    
    // Send buffer if it's not empty
    if (strlen(buffer) > 0) {
        send_string(buffer);
    }
    
    closedir(d); // Closes dir_fd
    return 0;
}

/*
 * Reads a file at the specified path.
 * If an offset is provided, reads from that offset.
 */
int op_read(char *args[], int arg_count) {
    char *path = args[0];
    long offset = 0;

    // Check arguments for offset
    if (arg_count == 2) {
        if (strncmp(args[0], "-offset=", 8) == 0) {
            offset = atol(args[0] + 8);
            path = args[1];
        } else {
            send_string("err-Usage: read [-offset=<num>] <path>");
            return -1;
        }
    } else if (arg_count != 1) {
        send_string("err-Usage: read [-offset=<num>] <path>");
        return -1;
    }

    if (check_path_mine(path) != 0) {
        send_string("err-Invalid path");
        return -1;
    }

    char resolved[2048];
    resolve_path(current_dir_path, path, resolved);

    // Get file lock and acquire reader lock
    FileLock *lock = get_file_lock(resolved);
    reader_lock(lock);

    // Open file
    int fd = openat(current_dir_fd, path, O_RDONLY);
    if (fd == -1) {
        reader_unlock(lock);
        release_file_lock(lock);
        send_string("err-Error reading file");
        return -1;
    }

    // Seek to offset if provided
    if (offset > 0) {
        if (lseek(fd, offset, SEEK_SET) == -1) {
            reader_unlock(lock);
            release_file_lock(lock);
            close(fd);
            send_string("err-Error seeking file");
            return -1;
        }
    }

    // Send header (ok-)
    send_string("ok-\n");

    // Read file and send it to client
    char buf[1024];
    int n;
    char last_char = '\n';
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        write(sockfd, buf, n);
        last_char = buf[n-1];
    }
    
    // Add newline if last character was not a newline
    if (last_char != '\n') {
        write(sockfd, "\n", 1);
    }
    
    // Close file and release lock
    close(fd);
    reader_unlock(lock);
    release_file_lock(lock);
    return 0;
}

/*
 * Writes to a file at the specified path.
 * If an offset is provided, writes from that offset.
 */
int op_write(char *args[], int arg_count) {
    char *path = args[0];
    long offset = 0;
    int has_offset = 0;

    // Check arguments for offset
    if (arg_count == 2) {
        if (strncmp(args[0], "-offset=", 8) == 0) {
            offset = atol(args[0] + 8);
            path = args[1];
            has_offset = 1;
        } else {
            send_string("err-Usage: write [-offset=<num>] <path>");
            return -1;
        }
    } else if (arg_count != 1) {
        send_string("err-Usage: write [-offset=<num>] <path>");
        return -1;
    }

    if (check_path_mine(path) != 0) {
        send_string("err-Invalid path");
        return -1;
    }
    char resolved[2048];
    resolve_path(current_dir_path, path, resolved);

    // Get file lock and acquire writer lock
    FileLock *lock = get_file_lock(resolved);
    writer_lock(lock);

    // if offset is not provided, truncate file
    int flags = O_WRONLY | O_CREAT;
    if (!has_offset) {
        flags |= O_TRUNC;
    }
    
    // if the file does not exist it is created with permission 0700
    int fd = openat(current_dir_fd, path, flags, 0700);
    if (fd == -1) {
        writer_unlock(lock);
        release_file_lock(lock);
        send_string("err-Error opening file for writing");
        return -1;
    }

    // if offset is provided, seek to offset
    if (has_offset && offset > 0) {
        if (lseek(fd, offset, SEEK_SET) == -1) {
            writer_unlock(lock);
            release_file_lock(lock);
            close(fd);
            send_string("err-Error seeking file");
            return -1;
        }
    }

    send_string("ok-Waiting for data... (Type 'EOF' to finish)");

    // receive data from client and write to file
    char buf[1024];
    int n;
    while ((n = recv(sockfd, buf, sizeof(buf), 0)) > 0) {
        if (n >= 4 && strncmp(buf, "EOF\n", 4) == 0) {
            break;
        }
        if (write(fd, buf, n) != n) {
            writer_unlock(lock);
            release_file_lock(lock);
            close(fd);
            perror("write failed");
            break; 
        }
    }
    
    // close file and release lock
    close(fd);
    writer_unlock(lock);
    release_file_lock(lock);

    send_string("ok-File written successfully.");
    return 0;
}

/*
 * Deletes a file or directory if it is empty
 */
int op_delete(char *args[], int arg_count) {
    if (arg_count != 1) {
        send_string("err-Usage: delete <path>");
        return -1;
    }
    
    char *path = args[0];
    
    if (check_path_mine(path) != 0) {
        send_string("err-Invalid path");
        return -1;
    }

    char resolved[2048];
    resolve_path(current_dir_path, path, resolved);

    // Get file lock and acquire writer lock
    FileLock *lock = get_file_lock(resolved);
    writer_lock(lock);

    // Try to remove as file
    if (unlinkat(current_dir_fd, path, 0) == -1) {
        // If it fails because it is a directory, try to remove as directory
        if (errno == EISDIR) {
            if (unlinkat(current_dir_fd, path, AT_REMOVEDIR) == -1) {
                perror("unlinkat dir failed");
                send_string("err-Error deleting directory");
                writer_unlock(lock);
                release_file_lock(lock);
                return -1;
            }
        } else {
            writer_unlock(lock);
            release_file_lock(lock);
            perror("unlinkat file failed");
            send_string("err-Error deleting file");
            return -1;
        }
    }
    
    // close file and release lock
    writer_unlock(lock);
    release_file_lock(lock);

    char msg[256];
    snprintf(msg, sizeof(msg), "ok-Deleted %s.", path);
    send_string(msg);

    printf("[PID: %d] Deleted %s\n", getpid(), path);
    return 0;
}

/*
 * Sends a transfer request to the destination user.
 */
int op_transfer_request(char *args[], int arg_count){
    char path[PATH_LENGTH];
    
    if(arg_count != 2){
        send_string("err-Usage: transfer_request <path> <dest_user>");
        return -1;
    }

    if(check_path_mine(args[0]) != 0){
        send_string("err-Invalid path");
        return -1;
    }

    if(user_exists(args[1])){
        send_string("err-Invalid user");
        return -1;
    }
    resolve_path(current_dir_path, args[0], path);

    // Get file lock and acquire reader lock
    FileLock *lock = get_file_lock(path);
    reader_lock(lock);

    // create request
    create_request(username, path, args[1]);

    // close file and release lock
    reader_unlock(lock);
    release_file_lock(lock);
    
    return 0;
}

/*
 * Accepts a transfer request.
 */
int op_accept(char *args[], int arg_count){
    char path[PATH_LENGTH];
    
    if(arg_count != 2){
        send_string("err-Usage: accept <req_id> <dest_path>");
        return -1;
    }

    int id = atoi(args[1]);
    if(id < 0){
        send_string("err-Invalid request id");
        return -1;
    }

    if(check_path_mine(args[0]) != 0){
        send_string("err-Invalid path");
        return -1;
    }

    resolve_path(current_dir_path, args[0], path);
    accept_req(id, path);

    return 0;
}

/*
 * Rejects a transfer request.
 */
int op_reject(char *args[], int arg_count){
    if(arg_count != 1){
        send_string("err-Usage: reject <req_id>");
        return -1;
    }

    int id = atoi(args[0]);
    if(id < 0){
        send_string("err-Invalid request id");
        return -1;
    }

    reject_req(id);

    return 0;
}