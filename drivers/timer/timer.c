#include "timer.h"
#include "../../arch/mmio.h"
#include "../../libs/scheduler.h"

// Il periodo del tick e' la costante TIMER_PERIOD definita in timer.h
unsigned int curVal = 0;

void timer_init(void) {
  curVal = mmio_read(TIMER_CLO);
  curVal += TIMER_PERIOD;
  mmio_write(TIMER_C1, curVal);
}

void handle_timer_irq(void) {
  mmio_write(TIMER_CS, TIMER_CS_M1);
  // Riarmo relativo a ORA (CLO), non alla scadenza precedente: il System Timer
  // scatta quando CLO e' UGUALE a C1, quindi se questo gestore viene eseguito
  // in ritardo di piu' di un periodo (stampe lente, jitter dell'emulatore) un
  // incremento assoluto metterebbe C1 nel passato e il timer non sparerebbe
  // mai piu'. Col periodo a 10 ms il ritardo e' facile da superare al boot
  curVal = mmio_read(TIMER_CLO) + TIMER_PERIOD;
  mmio_write(TIMER_C1, curVal);
  handle_timer_tick();
}
