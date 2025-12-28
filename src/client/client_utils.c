#include "common.h"
#include "client.h"

extern int sockfd;

struct termios orig_termios;
char input_buffer[BUFFER_SIZE];
int input_len;

void disableRawMode() { tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios); }

void enableRawMode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disableRawMode);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void refresh_line() {
    printf("\r\033[K");
    printf("> ");
    printf("%.*s", input_len, input_buffer);
    fflush(stdout);
}

int handle_server_message() {
    static int incomplete_line = 0;
    char recv_buffer[BUFFER_SIZE];
    memset(recv_buffer, 0, BUFFER_SIZE);
    int valread = read(sockfd, recv_buffer, BUFFER_SIZE - 1);
    if (valread <= 0)
        return 0; // Server disconnected or error

    recv_buffer[valread] = '\0'; // Ensure null termination

    if (!incomplete_line) {
        printf("\r\033[K");
        printf("Server: ");
    }
    printf("%s", recv_buffer);

    if (valread > 0 && recv_buffer[valread - 1] == '\n') {
        incomplete_line = 0;
        refresh_line(); // Restores your partial typing!
    } else {
        incomplete_line = 1;
        fflush(stdout);
    }
    return 1;
}

void handle_user_input() {
    char c;
    if (read(STDIN_FILENO, &c, 1) == 1) {
        if (c == '\n') {
            // User pressed Enter: send the message
            input_buffer[input_len] = '\n';
            printf("\r\033[KYou: %.*s", input_len + 1, input_buffer);
            int ret = op_command(input_buffer);
            if (ret == 0)
                send(sockfd, input_buffer, input_len + 1, 0);

            // Print "You: ..." locally
            
            
            
            // Reset input buffer
            input_len = 0;
            memset(input_buffer, 0, BUFFER_SIZE);
            refresh_line();
        } else if (c == 127 || c == 8) {
            // Handle Backspace
            if (input_len > 0) {
                input_len--;
                refresh_line();
            }
        } else {
            // Regular character: add to buffer and echo
            if (input_len < BUFFER_SIZE - 2) {
                input_buffer[input_len++] = c;
                putchar(c);
                fflush(stdout);
            }
        }
    }
}