#ifndef TRANSFER_H
#define TRANSFER_H

#include "server.h"
#include "common.h"
#include "users.h"


#define PATH_LENGTH 1024

extern int pipe_read;
extern int pipe_write;
extern char username[USERNAME_LENGTH];
extern ClientSession sessions[];

typedef struct {
    int id;
    char sender[USERNAME_LENGTH];
    char receiver[USERNAME_LENGTH];
    char path[PATH_LENGTH];
    
} transfer_request;

typedef enum {
    TRANSF_REQ,     //0
    ACCEPT,         //1
    REJECT,         //2
    I_M_USER,       //3
    NEW_REQ,        //4
    HANDLED,        //5
    WHO_ARE_YOU     //6
} transfer_status;

typedef struct{
    transfer_status status;
    transfer_request req;
} transfer_msg;

typedef struct req_list{
    transfer_request req;
    struct req_list *next;
} req_list;

typedef struct dict{
    int key;
    int value;
    struct dict *next;
} dict;


int create_request(char *sender, char *receiver, char *path);
int accept_req(int id, char *dest);
int reject_req(int id);
int i_am_user();
int child_handle_msg();
int parent_handle_msg(int i);



#endif