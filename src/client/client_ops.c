#include "common.h"
#include "client.h"


int op_command(char *command) {
    
    printf("I execute the command: %s\n", command);
    if (strncmp(command, "download ",9)==0)
        op_download(command);
    if (strncmp(command, "upload ",7)==0)
        op_upload(command);
    if (strcmp(command, "exit")==0)
        exit(0);
    
    
    return 0;
    
}   


int op_download(char *command) {
    
    
    return 0;
}

int op_upload(char *command) {
    
    
    return 0;
}
