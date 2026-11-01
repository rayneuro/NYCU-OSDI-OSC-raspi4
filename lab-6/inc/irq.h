#ifndef IRQ_H
#define IRQ_H
#include <stdint.h>
#include "address.h"
// 1. Set ARM Local register base Pi4 to 0xFF800000
#define ARM_LOCAL_BASE (KERNEL_BASE + 0xFF800000UL)
#define CORE0_TIMER_IRQ_CTRL    ((volatile unsigned int*)(ARM_LOCAL_BASE + 0x40))
#define CORE0_INTERRUPT_SOURCE  ((volatile unsigned int*)(ARM_LOCAL_BASE + 0x60))

// 2. ARM GIC-400 暫存器定義 (取代原本的 0xB200)
#define GIC_BASE_ADDRESS        (KERNEL_BASE + 0xFF840000UL)
#define GIC_DIST_BASE           (KERNEL_BASE + 0xFF841000UL)  // Distributor 基底
#define GIC_CPU_BASE            (KERNEL_BASE + 0xFF842000UL)  // CPU Interface 基底

#define GICD_ISENABLER(n)       ((volatile unsigned int*)(GIC_DIST_BASE + 0x100 + (n)*4))
#define GICC_IAR                ((volatile unsigned int*)(GIC_CPU_BASE + 0x00C))
#define GICC_EOIR               ((volatile unsigned int*)(GIC_CPU_BASE + 0x010))

#define GICD_CTLR       ((volatile uint32_t *)(GIC_DIST_BASE + 0x000))
#define GICD_IPRIORITYR ((volatile uint32_t *)(GIC_DIST_BASE + 0x400))
#define GICD_ITARGETSR  ((volatile uint32_t *)(GIC_DIST_BASE + 0x800))

#define GICC_CTLR       ((volatile uint32_t *)(GIC_CPU_BASE + 0x000))
#define GICC_PMR        ((volatile uint32_t *)(GIC_CPU_BASE + 0x004))

#define GIC_CNTNS_IRQ_ID 30U

uint64_t irq_save(void);
void irq_restore(uint64_t flags);
void gic_init(void);


#endif
