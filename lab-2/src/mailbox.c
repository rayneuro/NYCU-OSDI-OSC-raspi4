#include "mailbox.h"
#include "gpio.h"
#include "type.h"
#include "framebuffer.h"



int mailbox_call ( unsigned char channel, volatile uint32_t * mail_box )
{   
    const uint32_t interface = ((unsigned int)((unsigned long)mail_box)&~0xF) | (channel & 0xF);

    /* wait until  the full flag is not set */
    do
    {
        asm volatile("nop");

    } while ( mmio_read(MAILBOX_REG_STATUS) & MAILBOX_FULL );

    /* write the address of our message to the mailbox with channel identifier */
    mmio_write(MAILBOX_REG_WRITE , interface);

    while(1)
    {
        /* check if the response is exist */
        do
        {
            asm volatile("nop");
        
        } while( mmio_read(MAILBOX_REG_STATUS) & MAILBOX_EMPTY );

        /** check is our message or not */
        if( mmio_read(MAILBOX_REG_READ) == interface )
        {
            /* is it a valid successful response? */
            return mail_box[1] == TAGS_REQ_SUCCEED;
        }
    }

    return 0;
}


void get_board_revision(){
  unsigned int mailbox[7];
  mailbox[0] = 7 * 4; // buffer size in bytes
  mailbox[1] = MBOX_REQUEST;
  // tags begin
  mailbox[2] = GET_BOARD_REVISION; // tag identifier
  mailbox[3] = 4; // maximum of request and response value buffer's length.
  mailbox[4] = TAGS_REQ_CODE;
  mailbox[5] = 0; // value buffer
  // tags end
  mailbox[6] = TAGS_REQ_END;

  mailbox_call(MAILBOX_CH_PROP, mailbox); // message passing procedure call, you should implement it following the 6 steps provided above.

  printf("0x%x\n", mailbox[5]); // it should be 0xa020d3 for rpi4 b
}

