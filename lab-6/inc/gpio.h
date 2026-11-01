#ifndef GPIO_H
#define GPIO_H

enum {
    PERIPHERAL_BASE = 0xFE000000,
    GPFSEL0 = PERIPHERAL_BASE + 0x200000,
    GPFSEL1 = PERIPHERAL_BASE + 0x200004,
    GPFSEL2 = PERIPHERAL_BASE + 0x200008,
    GPFSEL3 = PERIPHERAL_BASE + 0x20000c,
    GPFSEL4 = PERIPHERAL_BASE + 0x200010,
    GPFSEL5 = PERIPHERAL_BASE + 0x200014,
    GPSET0 = PERIPHERAL_BASE + 0x20001C,
    GPFSET1 = PERIPHERAL_BASE + 0x200004,
    GPCLR0 = PERIPHERAL_BASE + 0x200028,
    GPLEV0 = PERIPHERAL_BASE + 0x200034,
    GPLEV1 = PERIPHERAL_BASE + 0x200038,
    GPEDS0 = PERIPHERAL_BASE + 0x200040,
    GPEDS1 = PERIPHERAL_BASE + 0x200044,
    GPHEN0 = PERIPHERAL_BASE + 0x200064,
    GPHEN1 = PERIPHERAL_BASE + 0x200068,
    GPPUPPDN0 = PERIPHERAL_BASE + 0x2000E4,
};

enum {
    GPIO_MAX_PIN       = 53,
    GPIO_FUNCTION_OUT  = 1,
    GPIO_FUNCTION_ALT5 = 2,
    GPIO_FUNCTION_ALT3 = 7,
    GPIO_FUNCTION_ALT0 = 4,
};

enum {
    Pull_None = 0,
    Pull_Down = 2,
    Pull_Up = 1
};


void mmio_write(unsigned int reg, unsigned int val) ;
unsigned int mmio_read(unsigned int reg) ; 
unsigned int gpio_set(unsigned int , unsigned int );
unsigned int gpio_clear(unsigned int , unsigned int);
unsigned int gpio_pull(unsigned int , unsigned int ); 
unsigned int gpio_function (unsigned int, unsigned int);
void gpio_useAsAlt3(unsigned int );
void gpio_useAsAlt5(unsigned int pin_number);
void gpio_useAsAlt0(unsigned int pin_number);

#endif