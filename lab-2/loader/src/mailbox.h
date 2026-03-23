
#ifndef _MAILBOX_H_
#define _MAILBOX_H_

extern volatile unsigned int mbox[36];
// Mailbox registers
enum{
    MMIO_BASE = 0xFE000000,
    MAILBOX_BASE = MMIO_BASE + 0xb880,
    MAILBOX_READ = MMIO_BASE,
    MAILBOX_STATUS = MMIO_BASE + 0x18 ,
    MAILBOX_WRITE = MAILBOX_BASE + 0x20,
    MAILBOX_EMPTY = 0x40000000,
    MAILBOX_FULL  = 0x80000000
    
};

// Mailboxes channels : https://github.com/raspberrypi/firmware/wiki/Mailboxes
enum{
    MBOX_CH_POWER = 0,
    MBOX_CH_FB   = 1,
    MBOX_CH_VUART = 2,
    MBOX_CH_VCHIQ = 3,
    MBOX_CH_LEDS  = 4,
    MBOX_CH_BTNS  = 5,
    MBOX_CH_TOUCH = 6,
    MBOX_CH_COUNT = 7,
    MBOX_CH_PROP  = 8,
};

// Tags : https://github.com/raspberrypi/firmware/wiki/Mailbox-property-interface
enum{
    MBOX_TAG_GETSERIAL  =    0x10004,
    MBOX_TAG_SETCLKRATE = 0x38002,
    MBOX_TAG_LAST   = 0,
    
};


int mbox_call(unsigned char ch);
#endif
