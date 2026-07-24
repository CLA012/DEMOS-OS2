#include "../common/user_syscalls.h"
#include "../common/testlib.h"
#include <stddef.h>

// MLFQ demonstration (run with active_algorithm = &sched_mlfq).
// To be read together with the [MLFQ] traces printed by the kernel (ML_TRACE):
// - child A: CPU-bound from level 0 -> chain of demotions towards the bottom
//   (OSTEP Rule 4a: whoever consumes the whole quantum moves down a level)
// - child B: "interactive" (short bursts + yield before exhausting the
//   quantum) -> no demotion, it stays at level 0
// - child C: CPU-bound moved to level 3 before starting -> its demotions
//   start from the level of ITS OWN priority (3 -> 4)
// - every 3 seconds the BOOST trace shows everybody going back to level 0
//   (OSTEP Rule 5, anti-starvation)
void main() {
  int pids[3];

  call_syscall_write("[TEST MLFQ] osservare le tracce [MLFQ] del kernel\n");

  // A: CPU-bound from level 0 (chain of demotions)
  int pid = call_syscall_fork();
  if (pid == 0) {
    burn_cpu_ms(4000);
    call_syscall_write("[A cpu-bound L0] fine\n");
    call_syscall_exit();
  }
  pids[0] = pid;

  // B: interactive, bursts below the level-0 quantum (50 ms) then yield:
  // whoever does not exhaust the quantum is not demoted and stays on top
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

  // C: CPU-bound, moved to level 3 before the go (IPC barrier)
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
