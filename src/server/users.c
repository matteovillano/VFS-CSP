#include "common.h"
#include "users.h"
#include "server.h"



extern int root_dir_fd;
extern char root_dir_path[];

User users[MAX_USERS];
int user_count = 0;


void save_users_to_file() {
    restore_privileges();
    int fd = openat(root_dir_fd, USERS_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) { 
        perror("openat users file");
        return;
    }
    write(fd, &user_count, sizeof(int));
    write(fd, users, sizeof(User) * user_count);
    close(fd);
    minimize_privileges();
}

int user_exists(char *username) {
    //retrive_users();
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0) return 0;
    }
    return -1;
}

int create_user_persistance(char *username, int permissions) {
    restore_privileges();
    if (user_count >= MAX_USERS) return -1;
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0) return -1;
    }

    User new_user;
    new_user.id = user_count;
    strncpy(new_user.username, username, USERNAME_LENGTH);
    new_user.permissions = permissions;
    
    users[user_count++] = new_user;
    save_users_to_file();
    minimize_privileges();
    return 0;
}

int delete_user_persistance(char *username) {
    restore_privileges();
    int found = -1;
    for (int i=0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0) {
            found = i;
            break;
        }
    }
    if (found == -1) return -1;
    
    // Shift
    for (int i = found; i < user_count - 1; i++) {
        users[i] = users[i+1];
    }
    user_count--;
    save_users_to_file();
    minimize_privileges();
    return 0;
}

void retrive_users() {
    //restore_privileges();

    printf("Retrieving users...\n");
    char path[2048];
    resolve_path(root_dir_path, USERS_FILE, path);
    printf("Path: %s\n", path);
    int fd = open(path, O_RDONLY);
    if (fd == -1) return;

    if (read(fd, &user_count, sizeof(int)) != sizeof(int)) {
        user_count = 0;
        close(fd);
        return;
    }
    
    if (user_count > MAX_USERS) user_count = MAX_USERS;
    read(fd, users, sizeof(User) * user_count);
    close(fd);
    printf("Retrieved %d users.\n", user_count);
    //minimize_privileges();
}


int create_user_folder(char *username, int permissions) {
    restore_privileges();
    
    mode_t old_umask = umask(0);
    if (mkdirat(root_dir_fd, username, permissions) == -1) {
        perror("mkdirat failed");
        minimize_privileges();
        return -1;
    }
    umask(old_umask);

    struct passwd *pwd = getpwnam(username);
    if (pwd == NULL) {
        perror("getpwnam failed in create_user_folder");
        minimize_privileges();
        return -1;
    }

    if (fchownat(root_dir_fd, username, pwd->pw_uid, pwd->pw_gid, 0) == -1) {
        perror("fchownat failed");
        minimize_privileges();
        return -1;
    }

    minimize_privileges();
    return 0;
}

// Helper to recursively delete directory contents
int remove_directory_recursive(int parent_fd, char *name) {
    int dfd = openat(parent_fd, name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
    if (dfd == -1) {
        return unlinkat(parent_fd, name, 0); // Try deleting as file if not dir
    }

    DIR *d = fdopendir(dfd);
    if (!d) {
        close(dfd);
        return -1;
    }

    struct dirent *dir;
    while ((dir = readdir(d)) != NULL) {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue;
        
        struct stat st;
        if (fstatat(dfd, dir->d_name, &st, AT_SYMLINK_NOFOLLOW) == -1) continue;

        if (S_ISDIR(st.st_mode)) {
            remove_directory_recursive(dfd, dir->d_name);
        } else {
            unlinkat(dfd, dir->d_name, 0);
        }
    }
    closedir(d);
    
    // Now delete the directory itself
    int ret = unlinkat(parent_fd, name, AT_REMOVEDIR);
    return ret;
}

int delete_user_folder(char *username) {
    restore_privileges();
    // Attempt to remove recursively
    if (remove_directory_recursive(root_dir_fd, username) == -1) {
        perror("delete recursive failed");
        return -1;
    }
    minimize_privileges();
    return 0;
}


int create_os_user(char *username) {
    
    restore_privileges();
    if (geteuid() != 0) {
        printf("Not root, skipping OS user creation for %s\n", username);
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("Fork failed");
        minimize_privileges();
        return -1;
    }
    if (pid == 0) {
        // Child process
        char gid_str[16];
        snprintf(gid_str, sizeof(gid_str), "%d", getuid());
        execlp("useradd", "useradd", "-m", "-g", gid_str, username, NULL);
        perror("execlp useradd failed");
        return -1;
    } else {
        // Parent process
        minimize_privileges();
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
          return 0;
        } else {
            //error creating the user
            return -1;
        }
        
    }
}   

int delete_os_user(char *username) {
    
    restore_privileges();
    if (geteuid() != 0) {
        printf("Not root, skipping OS user deletion for %s\n", username);
        return 0;
    }
    pid_t pid = fork();
    if (pid < 0) {
        perror("Fork failed");
        minimize_privileges();
        return -1;
    }
    if (pid == 0) {
        // Child process
        // Redirect stderr to /dev/null to suppress "mail spool not found" and "home not owned" errors
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull != -1) {
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        
        execlp("userdel", "userdel", "-r", username, NULL);
        perror("execlp userdel failed");
        return -1;
    } else {
        // Parent process
        minimize_privileges();
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);
            // 0: Success
            // 6: Specified user doesn't exist (can happen if inconsistencies) - arguably success if goal is "user gone"
            // 12: Cannot remove home dir / mail spool issues
            if (exit_code == 0 || exit_code == 6 || exit_code == 12) {
                return 0;
            }
        }
        // error deleting the user
        return -1;
    }
}



int create_user(char *username, int permissions) {
    
    if (user_count >= MAX_USERS) {
        return -1;
    }
    if (user_exists(username) == 0) {
        printf("err-user already exists\n");
        return -1;
    }
    int os_user = create_os_user(username);
    int persisnce_user = create_user_persistance(username, permissions);
    int folder_user = create_user_folder(username, permissions);

    if (os_user < 0 || persisnce_user < 0 || folder_user < 0) {
        if(os_user == 0) {
            delete_os_user(username);
        }
        if(persisnce_user == 0) {
            delete_user_persistance(username);
        }
        if(folder_user == 0) {
            delete_user_folder(username);
        }
        printf("err-user not created\n");
        return -1;
    } else {
        printf("user created\n");
        return 0;
    }

    
    

}

int delete_user(char *username) {
    if (user_exists(username)) {
        printf("err-user does not exist\n");
        return -1;
    }
    int os_user = delete_os_user(username);
    int persisnce_user = delete_user_persistance(username);
    int folder_user = delete_user_folder(username);

    if (os_user < 0 || persisnce_user < 0 || folder_user < 0) {
        printf("err-user not deleted\n");
        return -1;
    } else {
        printf("user deleted\n");
        return 0;
    }
}