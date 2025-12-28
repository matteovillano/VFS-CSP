#include "server.h"
#include "common.h"
#include "users.h"
#include "transfer.h"
#include <fcntl.h>
#include <pwd.h>


dict *dict_head = NULL;

int append_dict(int key, int value){
    dict *new_dict = malloc(sizeof(dict));
    new_dict->key = key;
    new_dict->value = value;
    new_dict->next = dict_head;
    dict_head = new_dict;
    return 0;
}

int pop_dict(int key, int *value){
    dict *curr = dict_head;
    dict *prev = NULL;

    while (curr != NULL) {
        if (curr->key == key) {
            *value = curr->value;
            if (prev == NULL) {
                dict_head = curr->next;
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
        if (msg.status == REJECTED){
            send_string("Transfer request rejected\n");
            break;
        }else if (msg.status == HANDLED){
            send_string("Transfer request handled successfully\n");
            break;
        }
    }
    
    return 0;
}

int accept_req(int id, char *dest){
    transfer_msg msg;
    memset(&msg, 0, sizeof(msg));
    msg.status = ACCEPT;
    msg.req.id = id;
    strcpy(msg.req.sender, username);
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

int send_handled_msg(int session){
    transfer_msg msg;
    memset(&msg, 0, sizeof(msg));
    msg.status = HANDLED;
    msg.req.id = 0;
    strcpy(msg.req.path, "");
    strcpy(msg.req.sender, "");
    strcpy(msg.req.receiver, "");

    int ret = send_transfer_msg(sessions[session].pipe_fd_write, &msg);
    if (ret < 0){
        exit(EXIT_FAILURE);
    }

    return 0;
}
int send_rejected_msg(int session){
    transfer_msg msg;
    memset(&msg, 0, sizeof(msg));
    msg.status = REJECTED;
    msg.req.id = 0;
    strcpy(msg.req.path, "");
    strcpy(msg.req.sender, "");
    strcpy(msg.req.receiver, "");

    int ret = send_transfer_msg(sessions[session].pipe_fd_write, &msg);
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
//parent operations
int perform_transfer(char *source, char *dest, char *receiver){
    int src_fd, dest_fd;
    ssize_t nread;
    char buffer[4096];
    struct passwd *pwd;

    restore_privileges();

    printf("[PARENT] Transferring %s to %s for user %s\n", source, dest, receiver);

    // Open source file
    src_fd = open(source, O_RDONLY);
    if (src_fd < 0) {
        perror("[PARENT] Error opening source file");
        minimize_privileges();
        return -1;
    }

    // Open/Create destination file
    // We open with 0644 initially, or 0600.
    printf("[PARENT] Opening destination file %s\n", dest);
    dest_fd = open(dest, O_WRONLY | O_CREAT | O_TRUNC, 0664);
    if (dest_fd < 0) {
        perror("[PARENT] Error opening destination file");
        close(src_fd);
        minimize_privileges();
        return -1; // Or retry/handle
    }

    // Copy loop
    while ((nread = read(src_fd, buffer, sizeof(buffer))) > 0) {
        if (write(dest_fd, buffer, nread) != nread) {
            perror("[PARENT] Error writing to destination file");
            close(src_fd);
            close(dest_fd);
            minimize_privileges();
            return -1;
        }
    }

    if (nread < 0) {
        perror("[PARENT] Error reading source file");
    }

    close(src_fd);
    close(dest_fd);

    // Change ownership of the destination file to the receiver
    pwd = getpwnam(receiver);
    if (pwd != NULL) {
        if (chown(dest, pwd->pw_uid, pwd->pw_gid) < 0) {
            perror("[PARENT] Error changing ownership");
            // Not fatal to the transfer itself, but good to log
        }
    } else {
        printf("[PARENT] Warning: Receiver user '%s' not found, ownership not changed\n", receiver);
    }
    
    minimize_privileges();
    return 0;
}



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

    transfer_request item_req;

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

            append_dict(req.id, i);

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

            if (pop_req_id(msg.req.id, &item_req) == 0) {
                if (strcmp(msg.req.sender, item_req.receiver) == 0) {
                    perform_transfer(item_req.path, msg.req.path, item_req.receiver);
                    transfer_msg handled_msg;
                    handled_msg.status = HANDLED;
                    handled_msg.req.id = item_req.id;
                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        if (sessions[i].pid != -1 && strcmp(sessions[i].username, item_req.sender) == 0) {
                            send_transfer_msg(sessions[i].pipe_fd_write, &handled_msg);
                            break;
                        }
                    }
                    int resp_i;
                    pop_dict(msg.req.id, &resp_i);
                    send_handled_msg(resp_i);
                }else{
                    append_req(item_req);
                }
            }
            
            break;

        case REJECT:
            // Received a reject request from a child.
            // TODO: Forward to the sender child.
            
            if (pop_req_id(msg.req.id, &item_req) == 0) {
                if (strcmp(msg.req.sender, item_req.receiver) == 0) {
                    transfer_msg rejected_msg;
                    rejected_msg.status = REJECTED;
                    rejected_msg.req.id = item_req.id;
                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        if (sessions[i].pid != -1 && strcmp(sessions[i].username, item_req.sender) == 0) {
                            send_transfer_msg(sessions[i].pipe_fd_write, &rejected_msg);
                            break;
                        }
                    }
                    int resp_i;
                    pop_dict(msg.req.id, &resp_i);
                    send_rejected_msg(resp_i);
                }else{
                    append_req(item_req);
                }
            }
            
            
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
