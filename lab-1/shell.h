#ifndef SHELL_H
#define SHELL_H
#define MAX_BUFFER_LEN 128
enum SHELL_CHARACTER
{
    // ASCII control characters
    BACK_SPACE = 8,
    BACK_SPACE_2 = 127,
    LINE_FEED = 10,
    CARRIAGE_RETURN = 13,
    REGULAR_INPUT = 1000,
    NEW_LINE = 1001,
    UNKNOWN = -1,

};

void shell_init();
enum SHELL_CHARACTER parse_character(char ch);
void command_line_parser(enum SHELL_CHARACTER, char ch, char [] ,int *);


#endif