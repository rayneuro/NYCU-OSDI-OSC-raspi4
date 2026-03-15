#ifndef GPIO_H
#define GPIO_H
#define PERIPHERAL_BASE = 0xFE000000

#define GPFSEL0 = PERIPHERAL_BASE + 0x200000
#define GPFSEL1 = PERIPHERAL_BASE + 0x200004
#define GPFSEL2 = PERIPHERAL_BASE + 0x200008
#define GPFSEL3 = PERIPHERAL_BASE + 0x20000c
#define GPFSEL4 = PERIPHERAL_BASE + 0x200010
#define GPFSEL5 = PERIPHERAL_BASE + 0x200014
#define GPSET0 = PERIPHERAL_BASE + 0x20001C
#define GPFSET1 = PERIPHERAL_BASE + 0x200004
#define GPCLR0 = PERIPHERAL_BASE + 0x200028
#define GPLEV0 = PERIPHERAL_BASE + 0x200034
#define GPLEV1 = PERIPHERAL_BASE + 0x200038
#define GPEDS0 = PERIPHERAL_BASE + 0x200040
#define GPEDS1 = PERIPHERAL_BASE + 0x200044
#define GPHEN0 = PERIPHERAL_BASE + 0x200064
#define GPHEN1 = PERIPHERAL_BASE + 0x200068
#define GPPUPPDN0 = PERIPHERAL_BASE + 0x2000E4

enum {
    GPIO_MAX_PIN       = 53,
    GPIO_FUNCTION_OUT  = 1,
    GPIO_FUNCTION_ALT5 = 2,
    GPIO_FUNCTION_ALT3 = 7
};

enum {
    Pull_None = 0,
    Pull_Down = 2,
    Pull_Up = 1
};


unsigned int gpio_set(unsigned int , unsigned int );
unsigned int gpio_clear(unsigned int , unsigned int);
unsigned int gpio_pull(unsigned int , unsigned int ); 
unsigned int gpio_function (unsigned int, unsigned int);
void gpio_useAsAlt3(unsigned int );
void gpio_useAsAlt5(unsigned int pin_number);

#endif