#include "server.h"
#include "common.h"

extern char root_dir_path[];
extern char current_dir_path[];
extern char username[];
extern ClientSession sessions[];

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

void handle_sigchld(int sig) {
    (void)sig;
    int status;
    pid_t pid;

    // Reap all dead children
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("[Server] Child process %d died\n", pid);
        
        // Find and free session
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (sessions[i].pid == pid) {
                close(sessions[i].pipe_fd_read);
                close(sessions[i].pipe_fd_write);
                sessions[i].pid = -1;
                memset(sessions[i].username, 0, USERNAME_LENGTH);
                printf("[Server] Freed session slot %d\n", i);
                break;
            }
        }
    }
}

/*
 * find_path
 * ---------
 * Resolves the path of a given file descriptor using /proc/self/fd.
 * Useful for debugging or recovering the path of an open directory.
 */
int find_path(char* dest, int dest_size, int fd){
    char proc_path[64];
    ssize_t len;

    // 1. Construct the path to the symbolic link in /proc
    // /proc/self/fd/N refers to the Nth file descriptor of the current process
    snprintf(proc_path, sizeof(proc_path), "/proc/self/fd/%d", fd);

    // 2. Read the symbolic link
    // readlink does NOT null-terminate the string, so we must handle it.
    len = readlink(proc_path, dest, dest_size - 1);

    if (len == -1) {
        perror("readlink failed");
        return -1;
    }

    // 3. Null-terminate the string
    dest[len] = '\0';
    printf("Resolved path: %s\n", dest);
    return 0;
}


// Helper to canonicalize path lexically
/*
 * resolve_path
 * ------------
 * Lexically resolves a path against a base directory.
 * Handles '.' and '..' components to create a canonical absolute path.
 * DOES NOT access the filesystem (pure lexical analysis).
 */
int resolve_path(char *base, char *path, char *resolved) {
    char temp[2048];
    char *tokens[256];
    int token_count = 0;
    
    // Start with base
    strncpy(temp, base, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';
    
    // Add separator if needed
    if (temp[strlen(temp) - 1] != '/') {
        strncat(temp, "/", sizeof(temp) - strlen(temp) - 1);
    }
    
    // Append input path
    strncat(temp, path, sizeof(temp) - strlen(temp) - 1);
    
    // Tokenize
    char *saveptr;
    char *token = strtok_r(temp, "/", &saveptr);
    while (token != NULL) {
        if (strcmp(token, ".") == 0) {
            // Ignore
        } else if (strcmp(token, "..") == 0) {
            if (token_count > 0) {
                token_count--;
            }
        } else {
            tokens[token_count++] = token;
        }
        token = strtok_r(NULL, "/", &saveptr);
    }
    
    // Reconstruct
    resolved[0] = '\0';
    if (token_count == 0) {
         strcat(resolved, "/");
    } else {
        for (int i = 0; i < token_count; i++) {
            strcat(resolved, "/");
            strcat(resolved, tokens[i]);
        }
    }
    return 0;
}

int check_path(char *path) {
    char resolved[2048];

    if (path[0] == '/') {
        return -1; // absolute path not allowed
    }
    
    // Resolve current_dir_path + path
    resolve_path(current_dir_path, path, resolved);
    
    //printf("Checking path: %s -> %s (Root: %s)\n", path, resolved, root_dir_path);

    // Check if resolved path starts with root_dir_path
    size_t root_len = strlen(root_dir_path);
    if (strncmp(resolved, root_dir_path, root_len) == 0) {
        // Ensure it's an exact match or a subdirectory (prevent /root vs /root_sibling)
        if (resolved[root_len] == '\0' || resolved[root_len] == '/') {
            return 0;
        }
    }
    
    return -1;
 }

/*
 * check_path_mine
 * ---------------
 * Security Check: Verifies that the requested path falls within the 
 * logged-in user's dedicated directory.
 * Prevents users from accessing other users' files or system files.
 */
int check_path_mine(char *path){
    char my_path[2048];
    resolve_path(root_dir_path, username, my_path);
    char resolved[2048];

    if (path[0] == '/') {
        return -1; // absolute path not allowed
    }
    
    // Resolve current_dir_path + path
    resolve_path(current_dir_path, path, resolved);
    
    //printf("Checking path: %s -> %s (Root: %s)\n", path, resolved, root_dir_path);

    // Check if resolved path starts with root_dir_path
    size_t my_len = strlen(my_path);
    if (strncmp(resolved, my_path, my_len) == 0) {
        // Ensure it's an exact match or a subdirectory (prevent /root vs /root_sibling)
        if (resolved[my_len] == '\0' || resolved[my_len] == '/') {
            return 0;
        }
    }
    
    return -1;
}