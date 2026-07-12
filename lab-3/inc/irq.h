#ifndef IRQ_H
#define IRQ_H

// 1. Set ARM Local register base Pi4 to 0xFF800000
#define PERIPHERAL_BASE = 0xFE000000,
#define CORE0_TIMER_IRQ_CTRL    ((volatile unsigned int*)(MMIO + 0x40))
#define CORE0_INTERRUPT_SOURCE  ((volatile unsigned int*)(ARM_LOCAL_BASE + 0x60))

// 2. ARM GIC-400 暫存器定義 (取代原本的 0xB200)
#define GIC_BASE_ADDRESS        0xFF840000
#define GIC_DIST_BASE           0xFF841000  // Distributor 基底
#define GIC_CPU_BASE            0xFF842000  // CPU Interface 基底

#define GICD_ISENABLER(n)       ((volatile unsigned int*)(GIC_DIST_BASE + 0x100 + (n)*4))
#define GICC_IAR                ((volatile unsigned int*)(GIC_CPU_BASE + 0x00C))
#define GICC_EOIR               ((volatile unsigned int*)(GIC_CPU_BASE + 0x010))

#endif