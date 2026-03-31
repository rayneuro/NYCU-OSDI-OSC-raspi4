#include "mailbox.h"
#include "gpio.h"



int mbox_call(unsigned char ch, unsigned int  *mbox){

    //unsigned int r = (((unsigned int)((unsigned long)&mbox)& 0x0FFFFFFF0UL ) | (ch & 0xF));
    const unsigned int r = ((unsigned int)((unsigned long)mbox)&~0xF) | (ch& 0xF);
    /* wait until we can write to the mailbox */
    do{asm volatile("nop");}while( mmio_read(MAILBOX_STATUS) & MAILBOX_FULL);
    /* write the address of our message to the mailbox with channel identifier */
    mmio_write(MAILBOX_WRITE, r);
    /* now wait for the response */
    while(1) {
        /* is there a response? */
        do{asm volatile("nop");}while( mmio_read(MAILBOX_STATUS) & MAILBOX_EMPTY);
        /* is it a response to our message? */
        if(r == mmio_read(MAILBOX_READ))
            /* is it a valid successful response? */
            return mbox[1]== 0x80000000; // check if response is valid and successful
    }
    return 0;
    

}

