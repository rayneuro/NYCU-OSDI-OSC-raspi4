#ifndef COMMAND_H
#define COMMAND_H
void command_timestamp();
void command_hello();
void command_help();
void command_reboot();
void command_not_found(char * buf);
void command_vc_base_addr();
void command_board_revision();
void command_load_image();
#endif