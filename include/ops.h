#ifndef OPS_H
#define OPS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int op_create(char *args[], int arg_count);
int op_changemod(char *args[], int arg_count);
int op_move(char *args[], int arg_count);
int op_cd(char *args[], int arg_count);
int op_list(char *args[], int arg_count);
int op_read(char *args[], int arg_count);
int op_write(char *args[], int arg_count);
int op_delete(char *args[], int arg_count);

#endif