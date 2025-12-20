#ifndef CLIENT_H
#define CLIENT_H

void disableRawMode();
void enableRawMode();
void refresh_line();
int handle_server_message();
void handle_user_input();


#endif