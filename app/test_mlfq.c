#include "../common/user_syscalls.h"
#include "../common/testlib.h"
#include <stddef.h>

// Dimostrazione MLFQ (eseguire con active_algorithm = &sched_mlfq).
// Va letta insieme alle tracce [MLFQ] stampate dal kernel (ML_TRACE):
// - figlio A: CPU-bound dal livello 0 -> demotion a catena verso il fondo
//   (Rule 4a di OSTEP: chi consuma l'intero quanto scende di livello)
// - figlio B: "interattivo" (burst brevi + yield prima di esaurire il quanto)
//   -> nessuna demotion, resta al livello 0
// - figlio C: CPU-bound spostato al livello 3 prima di partire -> le sue
//   demotion partono dal livello della SUA priorita' (3 -> 4)
// - ogni 3 secondi la traccia BOOST mostra il ritorno di tutti al livello 0
//   (Rule 5 di OSTEP, anti-starvation)
void main() {
  int pids[3];

  call_syscall_write("[TEST MLFQ] osservare le tracce [MLFQ] del kernel\n");

  // A: CPU-bound dal livello 0 (demotion a catena)
  int pid = call_syscall_fork();
  if (pid == 0) {
    burn_cpu_ms(4000);
    call_syscall_write("[A cpu-bound L0] fine\n");
    call_syscall_exit();
  }
  pids[0] = pid;

  // B: interattivo, burst sotto il quanto del livello 0 (50 ms) poi yield:
  // chi non esaurisce il quanto non viene demotato e resta in cima
  pid = call_syscall_fork();
  if (pid == 0) {
    unsigned long start = call_syscall_get_time();
    while ((unsigned long)(call_syscall_get_time() - start) < 4000000) {
      burn_cpu_ms(20);
      call_syscall_yield();
    }
    call_syscall_write("[B interattivo L0] fine\n");
    call_syscall_exit();
  }
  pids[1] = pid;

  // C: CPU-bound, spostato al livello 3 prima del via (barriera IPC)
  pid = call_syscall_fork();
  if (pid == 0) {
    char go[MAX_MESSAGES_BODY_SIZE];
    call_syscall_receive_message(go);
    burn_cpu_ms(2000);
    call_syscall_write("[C cpu-bound L3] fine\n");
    call_syscall_exit();
  }
  pids[2] = pid;
  call_syscall_set_sched_param(pids[2], SCHED_PARAM_QUEUE_PRIORITY, 3);
  call_syscall_send_message(pids[2], "GO");

  for (int i = 0; i < 3; i++) call_syscall_wait(pids[i]);
  call_syscall_write("[TEST MLFQ] completato: A demotato a catena, B rimasto in cima,\n");
  call_syscall_write("[TEST MLFQ] C partito dal livello 3, boost ogni 3 secondi\n");
  call_syscall_exit();
}
