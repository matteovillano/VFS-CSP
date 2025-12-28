#include "common.h"
#include "client.h"

extern int sockfd;
struct termios orig_termios;
char input_buffer[BUFFER_SIZE];
int input_len;

/*
 * Disables raw mode for terminal input.
 */
void disableRawMode() { 
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios); 
}

/*
 * Enables raw mode for terminal input, sets stdin 
 */
void enableRawMode() {
    tcgetattr(STDIN_FILENO, &orig_termios); //stores the original settings in orig_termios
    atexit(disableRawMode); //ensures that disableRawMode is called on program exit
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO); //sets to 0 the flags ICANON and ECHO
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw); //applies the changes to stdin
}

/*
 * Refreshes the line, printing the input buffer.
 */ 
void refresh_line() {
    printf("\r\033[K"); // \r: Returns the cursor to the start of the current line, \033[K: Clear the line
    printf("> ");
    printf("%.*s", input_len, input_buffer);
    fflush(stdout);
}

/*
 * Handles server messages, printing them locally
 */
int handle_server_message() {
    static int incomplete_line = 0;
    char recv_buffer[BUFFER_SIZE];
    memset(recv_buffer, 0, BUFFER_SIZE);
    int valread = read(sockfd, recv_buffer, BUFFER_SIZE - 1);
    if (valread <= 0)
        return 0; // Server disconnected or error

    recv_buffer[valread] = '\0'; // Ensure null termination

    if (!incomplete_line) {
        printf("\r\033[KServer: ");
    }
    printf("%s", recv_buffer);

    if (valread > 0 && recv_buffer[valread - 1] == '\n') {
        incomplete_line = 0;
        refresh_line(); // Restores partial typing
    } else {
        incomplete_line = 1;
        fflush(stdout);
    }
    return 1;
}

/*
 * Handles user input, sending it to the server
 */
void handle_user_input() {
    char c;
    if (read(STDIN_FILENO, &c, 1) == 1) {
        if (c == '\n') {
            // if user presses enter, send the message
            input_buffer[input_len] = '\n';
            printf("\r\033[KYou: %.*s", input_len + 1, input_buffer);
            int ret = op_command(input_buffer);
            if (ret == 0)
                send(sockfd, input_buffer, input_len + 1, 0);
            
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