#include "common.h"

extern int sockfd;
// Global variables as specified
extern int root_dir_fd;
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
    if (stat(root_dir, &st) == -1) {
        if (mkdir(root_dir, 0777) == -1) {
            perror("mkdir");
            exit(EXIT_FAILURE);
        }
        printf("Created root directory: %s\n", root_dir);
    }
    mode_t old_umask = umask(0);
    int fd = open(root_dir, O_RDONLY | O_DIRECTORY);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }
    umask(old_umask);
    return fd;
}

int send_string(char *str){
    write(sockfd, str, strlen(str));
    if(strcspn(str, "\n") == strlen(str))
        write(sockfd, "\n", 1);
    return 0;
}


static uid_t real_uid = 0;
static gid_t real_gid = 0;
static int privileges_initialized = 0;

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

void minimize_privileges() {
    if (!privileges_initialized) return;

    if (seteuid(real_uid) != 0) {
        perror("seteuid failed");
    }
}

void restore_privileges() {
    if (!privileges_initialized) return;

    if (seteuid(0) != 0) {
        perror("seteuid(0) failed");
    }
}

uid_t get_real_uid() {
    return real_uid;
}

int check_path(char *path) {
    (void)path;
    return 0;
 }