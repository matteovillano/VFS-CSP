#include "common.h"
#include "server.h"
#include "users.h"
#include "ops.h"

extern int sockfd;
extern int current_dir_fd;

int check_permissions(char *perm_str) {
    if (perm_str == NULL || strlen(perm_str) != 3) {
        return -1;
    }
    for (int i = 0; i < 3; i++) {
        if (perm_str[i] < '0' || perm_str[i] > '7') {
            return -1;
        }
    }
    return 0;
}

int open_root_dir(char *root_dir) {
    struct stat st = {0};
    mode_t old_umask = umask(0);
    if (stat(root_dir, &st) == -1) {
        if (mkdir(root_dir, 0777) == -1) {
            perror("mkdir");
            exit(EXIT_FAILURE);
        }
        printf("Created root directory: %s\n", root_dir);
    }
    
    int fd = open(root_dir, O_RDONLY | O_DIRECTORY);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }
    umask(old_umask);
    return fd;
}

int send_string(char *str){
    size_t len = strlen(str);
    size_t sent = 0;
    while(sent < len){
        ssize_t n = write(sockfd, str + sent, len - sent);
        if(n < 0){
            perror("write");
            return -1;
        }
        sent += n;
    }

    if(strcspn(str, "\n") == len)
        write(sockfd, "\n", 1);
    return 0;
}


static uid_t real_uid = 0;
static gid_t real_gid = 0;
static int privileges_initialized = 0;

/*
 * init_privileges
 * ---------------
 * Captures the original SUDO_UID and SUDO_GID from environment variables.
 * Allows the server to drop privileges to the original user and restore them when needed.
 */
void init_privileges() {
    const char *sudo_uid = getenv("SUDO_UID");
    const char *sudo_gid = getenv("SUDO_GID");

    if (sudo_uid && sudo_gid) {
        real_uid = (uid_t)atoi(getenv("SUDO_UID"));
        real_gid = (gid_t)atoi(sudo_gid);
        privileges_initialized = 1;
        printf("Privilege management initialized. Real UID: %d, GID: %d\n", real_uid, real_gid);
    } else {
        printf("Warning: SUDO_UID/SUDO_GID not found. Running as current user.\n");
    }
}

/*
 * minimize_privileges
 * -------------------
 * Drops effective user ID to the non-root SUDO user.
 * Used for most server operations to enhance security.
 */
void minimize_privileges() {
    if (!privileges_initialized) return;

    if (seteuid(real_uid) != 0) {
        perror("seteuid failed");
    }
}

/*
 * restore_privileges
 * ------------------
 * Elevates effective user ID back to root (0).
 * Used for critical operations like creating users or modifying protected files.
 */
void restore_privileges() {
    if (!privileges_initialized) return;

    if (seteuid(0) != 0) {
        perror("seteuid(0) failed");
    }
}

uid_t get_real_uid() {
    return real_uid;
}

gid_t get_real_gid() {
    return real_gid;
}
