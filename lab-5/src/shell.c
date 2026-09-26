#include "command.h"
#include "shell.h"
#include "uart.h"
#include "string.h"
#include "tasklist.h"
#include "cpio.h"
#include "mm.h"
#include "thread.h"
#include "print.h"

extern void *_dtb_ptr;
static int timeout_args_pending = 0;
static int exec_pending = 0;
static int get_cpio_pending = 0;


static int parse_timeout_args(const char *input, uint64_t *seconds,
                              const char **message)
{
    uint64_t value = 0;
    int digits = 0;

    while (*input == ' ')
        ++input;

    while (*input >= '0' && *input <= '9') {
        uint64_t digit = (uint64_t)(*input - '0');

        if (value > (~(uint64_t)0 - digit) / 10)
            return 0;

        value = value * 10 + digit;
        ++input;
        ++digits;
    }

    if (!digits || *input != ' ')
        return 0;

    while (*input == ' ')
        ++input;

    if (!*input)
        return 0;

    *seconds = value;
    *message = input;
    return 1;
}

void shell_init(){
    int buffer_counter = 0;
    char input_char;
    char buffer[MAX_BUFFER_LEN];
    
    enum SHELL_CHARACTER input_parse;

    // line head
    uart_async_send("# ");

    // read char
    while(1)
    {
        /* Run UART/timer bottom halves created by the IRQ top halves. */
        execute_tasks();

        while (uart_async_read(&input_char)) {
            input_parse = parse_character( input_char );
            command_line_parser( input_parse, input_char, buffer ,&buffer_counter);
        }
        schedule();
    } 
    

}


void command_line_parser(enum SHELL_CHARACTER cp, char ch, char buf[] , int * counter){
    if(cp == UNKNOWN)
        return ;

    if(cp == BACK_SPACE){
        if ( (*counter) > 0 )
            (*counter)--;
        
        uart_write_char(0x08);
        uart_write_char(' ');
        uart_write_char(0x08);

    }
    else if(cp == NEW_LINE){
        int show_prompt = 1;

        // Typing Enter on concole (Putty or tty) == '\r' == ch
        uart_async_send("\n");
        
        if((*counter) == MAX_BUFFER_LEN){
            return ;
        }else{
            buf[(*counter)] = '\0';
            
            if (timeout_args_pending) {
                uint64_t seconds;
                const char *message;

                timeout_args_pending = 0;
                if (parse_timeout_args(buf, &seconds, &message))
                    command_timeout(message, seconds);
                else
                    uart_async_send("Usage: <seconds> <message>\n");
            }else if(get_cpio_pending){
                get_cpio_pending = 0;
                cpio_cat(buf);
            }else if (exec_pending){
                exec_pending = 0;
                cpio_exec(buf);
            }
            else if(!strcmp( buf,"help")) command_help();
            else if(!strcmp(buf, "thread_test")) thread_test();
            else if(!strcmp(buf, "preempt_test")) thread_preempt_test();
            else if(!strcmp(buf, "quantum"))
                printf("Quantum: %d ms\n", (int)thread_get_quantum_ms());
            else if(buf[0] == 'q' && buf[1] == 'u' && buf[2] == 'a' &&
                    buf[3] == 'n' && buf[4] == 't' && buf[5] == 'u' &&
                    buf[6] == 'm' && buf[7] == ' ') {
                unsigned int ms = 0;
                const char *arg = buf + 8;
                while (*arg >= '0' && *arg <= '9' && ms <= 1000)
                    ms = ms * 10 + (unsigned int)(*arg++ - '0');
                if (*arg || !thread_set_quantum_ms(ms))
                    printf("Usage: quantum <1..1000 ms>\n");
                else
                    printf("Quantum: %d ms\n", (int)thread_get_quantum_ms());
            }
            else if(!strcmp(buf, "hello")) command_hello();
            else if(!strcmp(buf, "timestamp")) command_timestamp();
            else if(!strcmp(buf, "SetTimeout")) {
                timeout_args_pending = 1;
                show_prompt = 0;
                uart_async_send("Seconds and message: ");
            }
            else if(!strcmp(buf, "reboot")) command_reboot();
            else if(!strcmp(buf, "boardvision")) command_board_revision();
            else if(!strcmp(buf, "VC address")) command_vc_base_addr();
            else if(!strcmp(buf, "loadimg")) command_load_image();
            else if(!strcmp(buf,"ls")) command_list_file();
            else if(!strcmp(buf,"cat")){
                get_cpio_pending = 1;
                show_prompt  = 0;
                uart_async_send("File:  ");
            }else if(!strcmp(buf,"exec")) {
                exec_pending = 1;
                show_prompt = 0;
                uart_async_send("Program: ");
            }
            else if(!strcmp(buf,"malloc")) command_malloc();
            else if(!strcmp(buf,"dtb")) command_dtb();
            else if(!strcmp(buf,"ma")) mm_init();
            else command_not_found(buf);
        }
        (*counter) =0;
        strset(buf, 0, MAX_BUFFER_LEN);
        if (show_prompt)
            uart_async_send("# ");

    }else if(cp == REGULAR_INPUT ){
        
        if ( *counter < MAX_BUFFER_LEN)
        {
            buf[*counter] = ch;
            uart_async_write(buf + *counter,1);
            (*counter) ++;
        }
    }
    

}


enum SHELL_CHARACTER parse_character(char c){
    if(c > 128 || c < 0)
        return UNKNOWN;
    if (c == BACK_SPACE || c == BACK_SPACE_2)
        return BACK_SPACE;
    else if (c == LINE_FEED || c == CARRIAGE_RETURN)
        return NEW_LINE;
    else
        return REGULAR_INPUT;

}


