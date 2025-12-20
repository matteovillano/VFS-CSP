#ifndef COMMON_H
#define COMMON_H


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/prctl.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <sys/select.h>

#define DEFAULT_PORT 8080
#define DEFAULT_IP "127.0.0.1"
#define BUFFER_SIZE 1024





int open_root_dir(char *root_dir);
int create_server_socket(int port);
int create_client_socket(const char *ip, int port);
int check_permissions(char *permissions);
int send_string(char *str);

void init_privileges();
void minimize_privileges();
void restore_privileges();
uid_t get_real_uid();

#endif