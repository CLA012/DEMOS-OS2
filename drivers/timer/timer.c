#include "timer.h"
#include "../../arch/mmio.h"
#include "../../libs/scheduler.h"

// The tick period is the TIMER_PERIOD constant defined in timer.h
unsigned int curVal = 0;

void timer_init(void) {
  curVal = mmio_read(TIMER_CLO);
  curVal += TIMER_PERIOD;
  mmio_write(TIMER_C1, curVal);
}

void handle_timer_irq(void) {
  mmio_write(TIMER_CS, TIMER_CS_M1);
  // Re-arm relative to NOW (CLO), not to the previous deadline: the System
  // Timer fires when CLO is EQUAL to C1, so if this handler runs more than one
  // period late (slow prints, emulator jitter) an absolute increment would put
  // C1 in the past and the timer would never fire again. With the 10 ms period
  // that delay is easy to exceed at boot
  curVal = mmio_read(TIMER_CLO) + TIMER_PERIOD;
  mmio_write(TIMER_C1, curVal);
  handle_timer_tick();
}
