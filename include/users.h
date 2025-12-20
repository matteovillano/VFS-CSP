#ifndef USERS_H
#define USERS_H

#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdio.h>
#include <pwd.h>

#define MAX_USERS 25
#define USERNAME_LENGTH 256
#define USERS_FILE "users.dat"

typedef struct {
    int id;
    char username[USERNAME_LENGTH];
    int permissions;
} User;

int create_user(char *username, int permissions);
int delete_user(char *username);
int create_user_persistance(char *username, int permissions);
int delete_user_persistance(char *username);
void retrive_users();
void save_users_to_file();
int create_user_folder(char *username, int permissions);
int delete_user_folder(char *username);
int create_os_user(char *username);
int delete_os_user(char *username);
int user_exists(char *username);


#endif