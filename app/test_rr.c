#include "../common/user_syscalls.h"
#include "../common/testlib.h"
#include <stddef.h>

// Round Robin test (run with active_algorithm = &sched_round_robin).
// Four CPU-bound children print a marker every ~150 ms of consumed CPU: with
// the 10-tick quantum (100 ms) preemption rotates among them, so the [A] [B]
// [C] [D] markers must come out interleaved. For comparison, with FCFS active
// all the [A]s would come out first, then the [B]s, the [C]s and the [D]s.
#define N_CHILDREN 4

void main() {
  int pids[N_CHILDREN];

  call_syscall_write("[TEST RR] quattro figli CPU-bound: attesi [A] [B] [C] [D] alternati\n");

  for (int i = 0; i < N_CHILDREN; i++) {
    // Per-child marker: the array is copied by the fork together with the
    // rest of the memory, so each child prints its own letter
    char marker[] = "[A]";
    marker[1] = (char)('A' + i);
    int pid = call_syscall_fork();
    if (pid == 0) {
      for (int k = 0; k < 10; k++) {
        burn_cpu_ms(150);
        call_syscall_write(marker);
      }
      call_syscall_write("\n");
      call_syscall_write(marker);
      call_syscall_write(" fine\n");
      call_syscall_exit();
    }
    pids[i] = pid;
  }

  for (int i = 0; i < N_CHILDREN; i++) call_syscall_wait(pids[i]);
  call_syscall_write("[TEST RR] completato: verificare l'alternanza dei marcatori\n");
  call_syscall_exit();
}
