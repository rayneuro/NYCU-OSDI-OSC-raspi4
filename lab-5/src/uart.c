#include "gpio.h"
#include "uart.h"
#include "mailbox.h"
#include "ctype.h"
#include "tasklist.h"

#define AUX_MU_BAUD(baud) ((AUX_UART_CLOCK/(baud*8))-1) // Set up for mini UART1

char uart_write_buffer[UART_BUFFER_SIZE];
char uart_read_buffer[UART_BUFFER_SIZE];
int uart_read_index =0;
int uart_read_head = 0;
int uart_write_index = 0;
int uart_write_head = 0;

static int uart_tx_task_pending = 0;

static int uart_queue_push(char *queue, int *tail, int head, char ch)
{
    int next = (*tail + 1) % UART_BUFFER_SIZE;

    if (next == head)
        return 0;

    queue[*tail] = ch;
    *tail = next;
    return 1;
}

static void uart_schedule_transmit(void)
{
    uint64_t flags = irq_save();

    if (!uart_tx_task_pending) {
        uart_tx_task_pending = 1;
        create_task(uart_transmit_handler, 2);
    }

    irq_restore(flags);
}

void uart_init() {
    /* initialize UART */
    mmio_write(UART0_CR , 0);         // turn off UART0

    mbox_set_clock_to_PL011();

    /* map UART0 to GPIO pins */
    gpio_useAsAlt0(14);
    gpio_useAsAlt0(15);


    mmio_write(UART0_ICR , 0x7FF);    // clear interrupts
    // baud rate = UART_CLK / (16 × (IBRD + FBRD / 64))
    mmio_write(UART0_IBRD , 2);       // 115200 baud
    mmio_write(UART0_FBRD , 0xB);       // Baud Rate = UARTCLK / (16 × (IBRD + FBRD/64)) , Divisor = UARTCLK / (16 × 115200) = 4,000,000 / (16 × 115200) ≈ 2.170138
    mmio_write(UART0_LCRH, (3 << 5) | (1 << 4)); // Set up WLEN（Word Length）3 -> 8 bits and FEN（FIFO Enable）= 1 → open TX/RX FIFO
    mmio_write(UART0_CR, (1 << 0) | (1 << 8) | (1 << 9) | (1 << 4));    // enable Tx, Rx, FIFO
}

unsigned int uart_isReadByteNotReady()  { return mmio_read(UART0_FR) & (1 << 4); }
unsigned int uart_isWriteByteNotReady() { return mmio_read(UART0_FR) & (1 << 5); }

unsigned char uart_readByte() {
    while (uart_isReadByteNotReady());
    return (unsigned char)mmio_read(UART0_DR);
}

void uart_writeByteBlockingActual(unsigned char ch) {
    for (;;) {
        uint64_t flags = irq_save();
        if (!uart_isWriteByteNotReady()) {
            mmio_write(UART0_DR, (unsigned int)ch);
            irq_restore(flags);
            return;
        }
        irq_restore(flags);
    }
}

void uart_write_char(unsigned char ch){
    uart_writeByteBlockingActual(ch);
}

void uart_puts(char * buffer){
    while (*buffer) {
       
       if (*buffer == '\n') uart_write_char('\r');
       uart_write_char(*buffer++);
       
    }
}

/**
 * Display a binary value in hexadecimal
 */
void uart_hex(unsigned int d) {
    unsigned int n;
    int c;
    uart_puts("0x");
    for(c=28;c>=0;c-=4) {
        // get highest tetrad
        n=(d>>c)&0xF;
        // 0-9 => '0'-'9', 10-15 => 'A'-'F'
        n+=n>9?0x57:0x30;
        uart_write_char(n);
    }
}

void uart_CR(){
    uart_write_char('\r');
}

int uart_getint()
{
    int input, output;

    output = 0;
    
    while( 1 ) 
    {
        input = uart_readByte();
        uart_write_char(input);

        if ( !isdigit( input ) )
            break;

        output = output * 10 + (input - '0');
        
    }

    return output;
}

void uart_enable_interrupt(){
    mmio_write(UART0_ICR, 0x7ff);
    // Enable RX and TX interrupt for UART0 

    // RX FIFO 與 receive timeout interrupt 
    mmio_write(UART0_IMSC, UART_RXIM | UART_RTIM);

    asm volatile("dsb sy" ::: "memory");

    /* UART0 INTID 153: ISENABLER[4], bit 25 */
    *GICD_ISENABLER(UART0_GIC_IRQ_ID / 32) =
        1U << (UART0_GIC_IRQ_ID % 32);

    asm volatile("dsb sy");
    asm volatile("isb");
}

void uart_irq_handler(void)
{
    uint32_t status = mmio_read(UART0_MIS);
    uint32_t mask = mmio_read(UART0_IMSC);

    /* Mask level-triggered sources before GIC EOIR and defer their work. */
    if (status & (UART_RXIM | UART_RTIM))
        mask &= ~(UART_RXIM | UART_RTIM);

    if (status & UART_TXIM)
        mask &= ~UART_TXIM;

    mmio_write(UART0_IMSC, mask);
    mmio_write(UART0_ICR,
               status & (UART_RXIM | UART_RTIM | UART_TXIM));
    asm volatile("dsb sy" ::: "memory");

    if (status & (UART_RXIM | UART_RTIM))
        create_task(uart_receive_handler, 1);

    if (status & UART_TXIM)
        uart_schedule_transmit();
}

void uart_receive_handler(void)
{
    /* Drain the complete PL011 RX FIFO into the software RX queue. */
    while (!(mmio_read(UART0_FR) & UART_RXFE)) {
        char data = (char)(mmio_read(UART0_DR) & 0xff);

        uart_queue_push(uart_read_buffer, &uart_read_index,
                        uart_read_head, data);
    }

    mmio_write(UART0_ICR, UART_RXIM | UART_RTIM);

    /* Re-enable RX only after its deferred task has drained the FIFO. */
    uint64_t flags = irq_save();
    mmio_write(UART0_IMSC,
               mmio_read(UART0_IMSC) | UART_RXIM | UART_RTIM);
    irq_restore(flags);

    asm volatile("dsb sy" ::: "memory");
}

void uart_transmit_handler(void)
{
    /* Fill the PL011 TX FIFO until it is full or the software queue is empty. */
    while (uart_write_head != uart_write_index &&
           !(mmio_read(UART0_FR) & UART_TXFF)) {
        mmio_write(UART0_DR,
                   (uint8_t)uart_write_buffer[uart_write_head]);
        uart_write_head = (uart_write_head + 1) % UART_BUFFER_SIZE;
    }

    uint64_t flags = irq_save();
    uint32_t mask = mmio_read(UART0_IMSC);

    uart_tx_task_pending = 0;

    if (uart_write_head != uart_write_index)
        mask |= UART_TXIM;
    else
        mask &= ~UART_TXIM;

    mmio_write(UART0_IMSC, mask);
    mmio_write(UART0_ICR, UART_TXIM);
    irq_restore(flags);

    asm volatile("dsb sy" ::: "memory");
}

int uart_async_read(char *buffer)
{
    uint64_t flags;

    if (!buffer)
        return 0;

    flags = irq_save();

    if (uart_read_head == uart_read_index) {
        irq_restore(flags);
        return 0;
    }

    *buffer = uart_read_buffer[uart_read_head];
    uart_read_head = (uart_read_head + 1) % UART_BUFFER_SIZE;

    irq_restore(flags);
    return 1;
}

void uart_async_write(const char *buffer, int length)
{
    uint64_t flags;
    int queued = 0;

    if (!buffer || length <= 0)
        return;

    flags = irq_save();

    for (int i = 0; i < length; ++i) {
        if (!uart_queue_push(uart_write_buffer, &uart_write_index,
                             uart_write_head, buffer[i]))
            break;
        queued = 1;
    }

    irq_restore(flags);

    if (queued)
        uart_schedule_transmit();
}

void uart_async_write_char(const char ch)
{
    uint64_t flags;

    flags = irq_save();
    
    uart_queue_push(uart_write_buffer, &uart_write_index,
                             uart_write_head, ch);

    irq_restore(flags);


    uart_schedule_transmit();
}

void uart_async_send(const char *str)
{
    const char *end = str;

    if (!str)
        return;

    while (*end)
        ++end;

    uart_async_write(str, (int)(end - str));
}
