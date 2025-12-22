#ifndef SERVER_MAIN_H
#define SERVER_MAIN_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "common.h"
#include <sys/select.h>
#include <dirent.h>

#define MAX_CLIENTS 10

typedef struct {
    pid_t pid;
    int pipe_fd_read;  // Parent reads from here (c_to_p[0])
    int pipe_fd_write; // Parent writes to here (p_to_c[1])
} ClientSession;

int handle_client(int server_socket);
int handle_user();
int execute_command(char *command);
int login(char *username);
int check_path(char *path);
int find_path(char* dest, int dest_size, int fd);
int resolve_path(char *base, char *path, char *resolved);


#endif