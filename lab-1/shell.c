#include "shell.h"
#include "io.h"
#include "command.h"
#include "string.h"


void shell_init(){
    int buffer_counter = 0;
    char input_char;
    char buffer[MAX_BUFFER_LEN];
    
    enum SHELL_CHARACTER input_parse;

    // line head
    uart_writeText("# ");

    // read char
    while(1)
    {
        input_char = uart_readByte();

        input_parse = parse_character( input_char );

        command_line_parser( input_parse, input_char, buffer ,&buffer_counter);
    }    

}


void command_line_parser(enum SHELL_CHARACTER cp, char ch, char buf[] , int * counter){
    if(cp == UNKNOWN)
        return ;

    if(cp == BACK_SPACE){
        if ( (*counter) > 0 )
            (*counter) --;
        uart_writeByteBlocking(ch);
        uart_writeByteBlocking(' ');
        uart_writeByteBlocking(ch);

    }
    else if(cp == NEW_LINE){
        uart_writeByteBlocking(ch);
        if((*counter) == UART_MAX_QUEUE){
            return ;
        }else{
            uart_writeByteBlocking('\0');
             
            if(!strcmp( buf,"help")) command_help();
            else if(!strcmp(buf, "hello")) command_hello();
            else if(!strcmp(buf, "timestamp")) command_timestamp();
            else if(!strcmp(buf, "reboot")) command_reboot();
            else command_not_found(buf);
        }
        (*counter) =0;
        strset(buf, 0, MAX_BUFFER_LEN);
        uart_writeText("# ");

    }else if(cp == REGULAR_INPUT ){
        uart_writeByteBlocking(ch);
        if ( *counter < MAX_BUFFER_LEN)
        {
            buf[*counter] = ch;
            (*counter) ++;
        }

    }
    

}


enum SHELL_CHARACTER parse_character(char c){
    if(c > 128 || c < 0)
        return UNKNOWN;
    if (c == BACK_SPACE)
        return BACK_SPACE;
    else if (c == LINE_FEED || c == CARRIAGE_RETURN)
        return NEW_LINE;
    else
        return REGULAR_INPUT;

}


