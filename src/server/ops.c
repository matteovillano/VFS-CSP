#include "ops.h"
#include "users.h"
#include "common.h"
#include "server.h"
#include <errno.h>

extern int root_dir_fd;
extern int current_dir_fd;
extern int sockfd;
extern char *username;



int op_create(char *args[], int arg_count) {
    char msg[256];
    
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
    if (check_path(target_path) != 0) {
        send_string("err-Invalid path");
        return -1;
    }

    if (strcmp(args[0], "-d") == 0) {
        if (arg_count < 3) {
            send_string("err-Usage: create -d <dirname> <permissions>");
            return -1;
        }
        if (check_permissions(args[2]) != 0){
            send_string("err-permission not valid");
            return -1;
        }
        mode_t mode = (mode_t)strtol(args[2], NULL, 8);

        if (mkdirat(current_dir_fd, args[1], mode) == -1) {
            send_string("err-Error creating directory");
            perror("Error creating directory");
            return -1;
        }

        snprintf(msg, sizeof(msg), "ok-Directory %s created successfully with permissions %o.", args[1], mode);
        send_string(msg);
    } else {
        if (check_permissions(args[1]) != 0){
            send_string("err-permission not valid");
            return -1;
        }
        mode_t mode = (mode_t)strtol(args[1], NULL, 8);

        int fd = openat(current_dir_fd, args[0], O_CREAT | O_WRONLY | O_EXCL, mode);
        if (fd == -1) {
            send_string("err-Error creating file");
            perror("Error creating file");
            return -1;
        }
        printf("File %s created successfully with permissions %o.\n", args[0], mode);

        close(fd);

        snprintf(msg, sizeof(msg), "ok-File %s created successfully with permissions %o.", args[0], mode);
        send_string(msg);
    }
    return 0;
}


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

    if (check_path(args[0]) != 0) {
        send_string("err-Invalid path");
        return -1;
    }

    if (fchmodat(current_dir_fd, args[0], mode, 0) == -1) {
        perror("chmod failed");
        send_string("err-Error changing permissions");
        return -1;
    }
    snprintf(msg, sizeof(msg), "ok-Permissions for %s changed to %o.", args[0], mode);
    send_string(msg);
    return 0;
}

int op_move(char *args[], int arg_count) {
    char msg[256];
    if (arg_count != 2) {
        send_string("err-Usage: move <source> <destination>");
        return -1;
    }

    if (check_path(args[0]) != 0) {
        send_string("err-Invalid path");
        return -1;
    }

    if (check_path(args[1]) != 0) {
        send_string("err-Invalid path");
        return -1;
    }

    if (renameat(current_dir_fd, args[0], current_dir_fd, args[1]) == -1) {
        perror("rename failed");
        send_string("err-Error moving file");
        return -1;
    }

    snprintf(msg, sizeof(msg), "ok-Moved %s to %s.", args[0], args[1]);
    send_string(msg);
    return 0;
}


int op_upload(char *args[],int arg_count){
    (void)args;
    (void)arg_count;
    return 0;
}
int op_download(char *args[],int arg_count){
    (void)args;
    (void)arg_count;
    return 0;
}


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
    
    send_string("ok-Directory changed successfully.");
    return 0;
}

int op_list(char *args[], int arg_count) {
    int dir_fd;
    if (arg_count == 0) {
        dir_fd = dup(current_dir_fd);
        if (dir_fd == -1) {
            perror("dup failed");
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

    while ((dir = readdir(d)) != NULL) {
        if (fstatat(dir_fd, dir->d_name, &st, 0) == -1) {
            continue;
        }
        
        snprintf(line, sizeof(line), "%s\tSize: %ld\tPerms: %o\n", 
                 dir->d_name, st.st_size, st.st_mode & 0777);
                 
        if (strlen(buffer) + strlen(line) < sizeof(buffer) - 1) {
            strcat(buffer, line);
        } else {
            send_string(buffer);
            buffer[0] = '\0';
            strcat(buffer, line); 
        }
    }
    
    if (strlen(buffer) > 0) {
        send_string(buffer);
    }
    
    closedir(d); // Closes dir_fd
    return 0;
}


int op_read(char *args[], int arg_count) {
    char *path = args[0];
    long offset = 0;

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

    if (check_path(path) != 0) {
        send_string("err-Invalid path");
        return -1;
    }

    int fd = openat(current_dir_fd, path, O_RDONLY);
    if (fd == -1) {
        send_string("err-Error reading file");
        return -1;
    }

    if (offset > 0) {
        if (lseek(fd, offset, SEEK_SET) == -1) {
            close(fd);
            send_string("err-Error seeking file");
            return -1;
        }
    }

    // Send header (ok-)
    // Doing manual write to avoid send_string adding newline if we want, 
    // but op_list used "ok-\n". Let's stick to consistency.
    send_string("ok-\n");

    char buf[1024];
    int n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        // Write directly to sockfd to avoid string processing issues (null termination, newlines)
        // with binary or large text chunks.
        write(sockfd, buf, n);
    }
    
    close(fd);
    return 0;
}


int op_write(char *args[], int arg_count) {
    char *path = args[0];
    long offset = 0;
    int has_offset = 0;

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

    if (check_path(path) != 0) {
        send_string("err-Invalid path");
        return -1;
    }

    int flags = O_WRONLY | O_CREAT;
    if (!has_offset) {
        flags |= O_TRUNC;
    }
    
    // Per spec: "if the file does not exist it is created with permission 0700"
    int fd = openat(current_dir_fd, path, flags, 0700);
    if (fd == -1) {
        send_string("err-Error opening file for writing");
        return -1;
    }

    if (has_offset && offset > 0) {
        if (lseek(fd, offset, SEEK_SET) == -1) {
            close(fd);
            send_string("err-Error seeking file");
            return -1;
        }
    }

    send_string("ok-Waiting for data... (Type 'EOF' to finish)");

    char buf[1024];
    int n;
    while ((n = recv(sockfd, buf, sizeof(buf), 0)) > 0) {
        if (n >= 4 && strncmp(buf, "EOF\n", 4) == 0) {
            break;
        }
        if (write(fd, buf, n) != n) {
            perror("write failed");
            break; 
        }
    }
    
    close(fd);
    return 0;
}

int op_delete(char *args[], int arg_count) {
    if (arg_count != 1) {
        send_string("err-Usage: delete <path>");
        return -1;
    }
    
    char *path = args[0];
    
    if (check_path(path) != 0) {
        send_string("err-Invalid path");
        return -1;
    }

    // Try to remove as file
    if (unlinkat(current_dir_fd, path, 0) == -1) {
        // If it fails because it is a directory, try to remove as directory
        if (errno == EISDIR) {
            if (unlinkat(current_dir_fd, path, AT_REMOVEDIR) == -1) {
                perror("unlinkat dir failed");
                send_string("err-Error deleting directory");
                return -1;
            }
        } else {
            perror("unlinkat file failed");
            send_string("err-Error deleting file");
            return -1;
        }
    }

    char msg[256];
    snprintf(msg, sizeof(msg), "ok-Deleted %s.", path);
    send_string(msg);
    return 0;
}
int op_transfer_request(char *args[],int arg_count){
    (void)args;
    (void)arg_count;
    return 0;
}
int op_accept(char *args[],int arg_count){
    (void)args;
    (void)arg_count;
    return 0;
}
int op_reject(char *args[],int arg_count){
    (void)args;
    (void)arg_count;
    return 0;
}