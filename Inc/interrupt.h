#ifndef AM32_INTERRUPT_H
#define AM32_INTERRUPT_H

#include "main.h"
#include "targets.h"

// Preserve nesting and keep ordinary memory accesses inside the critical section.
// WCH's existing enable/disable intrinsics do not include a compiler memory barrier.
static inline uint32_t saveAndDisableInterrupts(void)
{
#ifdef WCH
    uint32_t state;
    // QingKe gintenr mirrors MIE/MPIE; change only those interrupt-enable bits.
    __asm volatile ("csrrc %0, 0x800, %1" : "=r" (state) : "r" (0x88) : "memory");
    return state;
#else
    uint32_t state = __get_PRIMASK();
    __disable_irq();
    return state;
#endif
}

static inline void restoreInterrupts(uint32_t state)
{
#ifdef WCH
    __asm volatile ("csrw 0x800, %0" : : "r" (state) : "memory");
#else
    __set_PRIMASK(state);
#endif
}

#endif
