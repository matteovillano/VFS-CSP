#include "server.h"
#include "common.h"

extern char root_dir_path[];
extern char current_dir_path[];

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
    
    printf("Checking path: %s -> %s (Root: %s)\n", path, resolved, root_dir_path);

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