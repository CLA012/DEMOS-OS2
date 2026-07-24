#ifndef __TIMER_H
#define __TIMER_H

/**
 * System Timer driver: initialization and interrupt handling.
 * Offsets relative to MMIO_BASE (0x3F000000 on Raspberry Pi 3).
 */

// ==============================
// Timer registers
// ==============================

#define TIMER_BASE	0x00003000

#define TIMER_CS	(TIMER_BASE + 0x00)
#define TIMER_CLO	(TIMER_BASE + 0x04)
#define TIMER_CHI	(TIMER_BASE + 0x08)
#define TIMER_C0	(TIMER_BASE + 0x0C)
#define TIMER_C1	(TIMER_BASE + 0x10)
#define TIMER_C2	(TIMER_BASE + 0x14)
#define TIMER_C3	(TIMER_BASE + 0x18)

#define TIMER_CS_M1	(1 << 1)

// ==============================
// System tick period
// ==============================

// Duration of one tick in microseconds (the System Timer counts at 1 MHz):
// 10000 us = 10 ms, i.e. 100 ticks per second. All the scheduler time quanta
// are expressed in ticks, so this constant sets their real duration
// (e.g. Round Robin quantum = 10 ticks = 100 ms).
#define TIMER_PERIOD 10000 // 10ms

void timer_init(void);
void handle_timer_irq(void);

#endif // __TIMER_H

