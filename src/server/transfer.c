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

req_list *req_list_head = NULL;
int req_counter = 0;

int append_req(transfer_request req){
    req_list *new_req = malloc(sizeof(req_list));
    memcpy(&new_req->req, &req, sizeof(transfer_request));
    new_req->next = req_list_head;
    req_list_head = new_req;
    return 0;
}

int pop_req_username(char *usern, transfer_request *req) {
    req_list *curr = req_list_head;
    req_list *prev = NULL;

    while (curr != NULL) {
        if (strcmp(curr->req.receiver, usern) == 0) {
            memcpy(req, &curr->req, sizeof(transfer_request));
            if (prev == NULL) {
                req_list_head = curr->next;
            } else {
                prev->next = curr->next;
            }
            free(curr);
            return 0;
        }
        prev = curr;
        curr = curr->next;
    }
    return -1;
}

int pop_req_id(int id, transfer_request *req) {
    req_list *curr = req_list_head;
    req_list *prev = NULL;

    while (curr != NULL) {
        if (curr->req.id == id) {
            memcpy(req, &curr->req, sizeof(transfer_request));
            if (prev == NULL) {
                req_list_head = curr->next;
            } else {
                prev->next = curr->next;
            }
            free(curr);
            return 0;
        }
        prev = curr;
        curr = curr->next;
    }
    return -1;
}




// Helper function to write exactly 'n' bytes
int write_n(int fd, void *vptr, size_t n) {
    size_t nleft;
    ssize_t nwritten;
    const char *ptr;

    ptr = vptr;
    nleft = n;
    while (nleft > 0) {
        if ((nwritten = write(fd, ptr, nleft)) <= 0) {
            if(nwritten <= 0) perror("write");
            if (nwritten < 0 && errno == EINTR)
                nwritten = 0;   /* and call write() again */
            else
                return -1;      /* error */
        }
        nleft -= nwritten;
        ptr += nwritten;
    }
    return 0;
}

// Helper function to read exactly 'n' bytes
// Returns 0 on success (read n bytes), -1 on error, or number of bytes read if EOF occurred prematurely
int read_n(int fd, void *vptr, size_t n) {
    size_t nleft;
    ssize_t nread;
    char *ptr;

    ptr = vptr;
    nleft = n;
    while (nleft > 0) {
        if ((nread = read(fd, ptr, nleft)) < 0) {
            if (errno == EINTR)
                nread = 0;      /* and call read() again */
            else
                return -1;
        } else if (nread == 0) {
            break;              /* EOF */
        }
        nleft -= nread;
        ptr += nread;
    }
    return (n - nleft);         /* return >= 0 */
}


int send_transfer_msg(int fd, transfer_msg *msg){
    return write_n(fd, msg, sizeof(transfer_msg));
}

int receive_transfer_msg(int fd, transfer_msg *msg){
    int n = read_n(fd, msg, sizeof(transfer_msg));
    if (n < 0) return -1;
    if (n == 0) return 0; // EOF
    if ((size_t)n < sizeof(transfer_msg)) return -1; // Partial read treated as error for now
    return n;
}



//child operations
int create_request(char *sender, char *path, char *receiver){
    transfer_msg msg;
    memset(&msg, 0, sizeof(msg));
    msg.status = NEW_REQ;
    msg.req.id = 0;
    strcpy(msg.req.sender, sender);
    strcpy(msg.req.receiver, receiver);
    strcpy(msg.req.path, path);

    int ret = send_transfer_msg(pipe_write, &msg);
    if (ret < 0){
        perror("send_transfer_msg");
        exit(EXIT_FAILURE);
    }
    //printf("[CHILD] Request sent:\n id: %d\n sender: %s\n receiver: %s\n path: %s\n", msg.req.id, msg.req.sender, msg.req.receiver, msg.req.path);
    send_string("Transfer request sent successfully\n");
    send_string("Waiting for response... I'm blocking\n");
    while (1) {
        if (receive_transfer_msg(pipe_read, &msg) <= 0) {
            perror("receive_transfer_msg");
            exit(EXIT_FAILURE);
        }
        if (msg.status == HANDLED) break;
    }
    send_string("Transfer request handled successfully\n");
    
    return 0;
}

int accept_req(int id, char *dest){
    transfer_msg msg;
    memset(&msg, 0, sizeof(msg));
    msg.status = ACCEPT;
    msg.req.id = id;
    strcpy(msg.req.path, dest);

    int ret = send_transfer_msg(pipe_write, &msg);
    if (ret < 0){
        exit(EXIT_FAILURE);
    }

    return 0;
}

int reject_req(int id){
    transfer_msg msg;
    memset(&msg, 0, sizeof(msg));
    msg.status = REJECT;
    msg.req.id = id;
    strcpy(msg.req.path, "\0");
    strcpy(msg.req.sender, username);
    strcpy(msg.req.receiver, "\0");

    int ret = send_transfer_msg(pipe_write, &msg);
    if (ret < 0){
        exit(EXIT_FAILURE);
    }

    return 0;
}

int i_am_user(){
    transfer_msg msg;
    memset(&msg, 0, sizeof(msg));
    msg.status = I_M_USER;
    msg.req.id = 0;
    strcpy(msg.req.sender, username);
    strcpy(msg.req.receiver, "");
    strcpy(msg.req.path, "");

    int ret = send_transfer_msg(pipe_write, &msg);
    if (ret < 0){
        exit(EXIT_FAILURE);
    }

    return 0;
}

int send_handled_msg(int id){
    transfer_msg msg;
    memset(&msg, 0, sizeof(msg));
    msg.status = HANDLED;
    msg.req.id = id;
    strcpy(msg.req.path, "");
    strcpy(msg.req.sender, "");
    strcpy(msg.req.receiver, "");

    int ret = send_transfer_msg(pipe_write, &msg);
    if (ret < 0){
        exit(EXIT_FAILURE);
    }

    return 0;
}


int child_handle_msg(){

    printf("hellooo im child handle message function\n");
    transfer_msg msg;
    int ret = receive_transfer_msg(pipe_read, &msg);
    if (ret < 0){
        exit(EXIT_FAILURE);
    }
    if (ret == 0){
        printf("[CHILD]: pipe closed\n");
    }

    printf("Child received message:\n id: %d\n status: %d\n sender: %s\n receiver: %s\n path: %s\n", msg.req.id, msg.status, msg.req.sender, msg.req.receiver, msg.req.path);
    
    switch (msg.status){
        case TRANSF_REQ:
            char buf[2048];    
            snprintf(buf, sizeof(buf), "Transfer request id: %d from %s to %s for file %s\n", msg.req.id, msg.req.sender, msg.req.receiver, msg.req.path);
            send_string(buf);

            break;
        case WHO_ARE_YOU:
            i_am_user();
            
            break;
        default:
            perror("Invalid message status");
            return -1;
    }
    return 0;
}


//parent operations
int parent_handle_msg(int i){

    transfer_msg msg;
    int ret = receive_transfer_msg(sessions[i].pipe_fd_read, &msg);
    if (ret < 0){
        //perror("read");
        return -1;
    }
    if (ret == 0){
        //printf("[PARENT]: pipe closed\n");
        return -1;
    }
    
    printf("Parent received message:\n id: %d\n status: %d\n sender: %s\n receiver: %s\n path: %s\n", msg.req.id, msg.status, msg.req.sender, msg.req.receiver, msg.req.path);

    switch (msg.status){
        case NEW_REQ:
            printf("[MAIN] im handling a new request\n");
            transfer_request req;
            req.id = ++req_counter;
            strcpy(req.sender, msg.req.sender);
            strcpy(req.sender, sessions[i].username);
            strcpy(req.receiver, msg.req.receiver);
            strcpy(req.path, msg.req.path);
            append_req(req);

            transfer_msg who_are_you_msg;
            who_are_you_msg.status = WHO_ARE_YOU;
            for (int k = 0; k < MAX_CLIENTS; k++) {
                if (sessions[k].pid != -1) {
                    printf("[MAIN] im sending who are you to %d with fd %d\n", sessions[k].pid, sessions[k].pipe_fd_write);
                    send_transfer_msg(sessions[k].pipe_fd_write, &who_are_you_msg);
                }
            }

            // Received a new transfer request from a child.
            // TODO: Forward to the receiver child.
            //printf("Processing NEW_REQ from %s to %s file %s\n", msg.req.sender, msg.req.receiver, msg.req.path);
            break;

        case ACCEPT:    
            // Received an accept request from a child.
            // TODO: Forward to the sender child.
            printf("Processing ACCEPT from %s to %s file %s\n", msg.req.sender, msg.req.receiver, msg.req.path);
            break;

        case REJECT:
            // Received a reject request from a child.
            // TODO: Forward to the sender child.
            transfer_request item_req;
            if (pop_req_id(msg.req.id, &item_req) == 0) {
                if (strcmp(msg.req.sender, item_req.receiver) == 0) {
                    transfer_msg handled_msg;
                    handled_msg.status = HANDLED;
                    handled_msg.req.id = item_req.id;
                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        if (sessions[i].pid != -1 && strcmp(sessions[i].username, item_req.sender) == 0) {
                            send_transfer_msg(sessions[i].pipe_fd_write, &handled_msg);
                            break;
                        }
                    }
                }else{
                    append_req(item_req);
                }
            }
            
            send_handled_msg(msg.req.id);
            printf("Processing REJECT from %s to %s file %s\n", msg.req.sender, msg.req.receiver, msg.req.path);
            break;

        case I_M_USER:
            transfer_request myreq;
            int ret = pop_req_username(msg.req.sender, &myreq);
            
            if (ret == 0){
                append_req(myreq);
                transfer_msg req_msg;
                req_msg.status = TRANSF_REQ;
                req_msg.req.id = myreq.id;
                strcpy(req_msg.req.sender, myreq.sender);
                strcpy(req_msg.req.sender, sessions[i].username);
                strcpy(req_msg.req.receiver, myreq.receiver);
                strcpy(req_msg.req.path, myreq.path);
                int ret = send_transfer_msg(sessions[i].pipe_fd_write, &req_msg);
                if (ret < 0){
                    exit(EXIT_FAILURE);
                }
            }
            printf("Session %d (PID %d) identifies as user: %s\n", i, sessions[i].pid, msg.req.sender);
            break;

        default:
            printf("Unknown message status: %d\n", msg.status);
            return -1;
    }
    return 0;
}
