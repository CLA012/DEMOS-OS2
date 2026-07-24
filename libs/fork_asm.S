#include "scheduler.h"

.globl set_pgd
set_pgd:
    msr ttbr0_el1, x0
    tlbi vmalle1is
    DSB ISH
    isb
    ret

.globl cpu_switch_to_process
cpu_switch_to_process:
	// Save in X10 the offset of the register area inside the struct
    mov    x10, #CPU_CONTEXT_OFFSET_IN_PCB
    add    x8, x0, x10    // x8 holds the pointer to the register struct
    mov    x9, sp
    // Save all the registers into the old process structure
    stp    x19, x20, [x8], #16
    stp    x21, x22, [x8], #16
    stp    x23, x24, [x8], #16
    stp    x25, x26, [x8], #16
    stp    x27, x28, [x8], #16
    stp    x29, x9, [x8], #16
    str    x30, [x8]
    // Now point x8 to the register struct of the new process
    add    x8, x1, x10
    // Load the registers from the new process struct
    ldp    x19, x20, [x8], #16
    ldp    x21, x22, [x8], #16
    ldp    x23, x24, [x8], #16
    ldp    x25, x26, [x8], #16
    ldp    x27, x28, [x8], #16
    ldp    x29, x9, [x8], #16
    // Save in x30 the pointer to the new struct
    ldr    x30, [x8]
    mov    sp, x9
    // Returns the pointer to the new process
    ret

