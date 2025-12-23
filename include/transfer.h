#ifndef TRANSFER_H
#define TRANSFER_H

#include "server.h"
#include "common.h"
#include "users.h"


int create_request(char *sender, char *receiver, char *path);
int accept_req(int id, char *dest);
int reject_req(int id);
int i_am_user();
int child_handle_msg();
int parent_handle_msg(int i);



#endif