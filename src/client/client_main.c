#include "common.h"
#include "client.h"


int sockfd = -1;
char server_ip[INET_ADDRSTRLEN];
int server_port;




int main(int argc, char *argv[]) {

    strncpy(server_ip, DEFAULT_IP, INET_ADDRSTRLEN);
    server_port = DEFAULT_PORT;

    if (argc > 3) {
        fprintf(stderr, "Too many arguments\n");
        exit(EXIT_FAILURE);
    }

    if (argc >= 2)
        strncpy(server_ip, argv[1], INET_ADDRSTRLEN);
    if (argc >= 3)
        server_port = atoi(argv[2]);

    // connect to server
    sockfd = create_client_socket(server_ip, server_port);
    if (sockfd < 0)
        exit(EXIT_FAILURE);

    printf("Connected to server at %s:%d\n", server_ip, server_port);

    enableRawMode();
    fd_set read_fds;
    int max_fd;

    refresh_line();

    while(1){
        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);
        FD_SET(STDIN_FILENO, &read_fds);

        max_fd = sockfd;
        if (STDIN_FILENO > max_fd)
            max_fd = STDIN_FILENO;

        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0){
            if (errno == EINTR) continue;
            perror("select");
            exit(EXIT_FAILURE);
        }

        if (FD_ISSET(sockfd, &read_fds)) {
            if (handle_server_message() == 0) {
                printf("\nServer disconnected.\n");
                break;
            }
        }
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            handle_user_input(sockfd);
        }
            


    }

    disableRawMode();
    close(sockfd);
    
    return 0;
}