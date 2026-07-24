#include "../common/user_syscalls.h"
#include "../common/testlib.h"
#include <stddef.h>

// Round Robin test (run with active_algorithm = &sched_round_robin).
// Two CPU-bound children print a marker every ~150 ms of consumed CPU: with
// the 10-tick quantum (100 ms) preemption alternates them, so the [A] and [B]
// markers must come out interleaved. For comparison, with FCFS active all the
// [A]s would come out first, then all the [B]s.
void main() {
  call_syscall_write("[TEST RR] due figli CPU-bound: attesi [A] e [B] alternati\n");

  int pid_a = call_syscall_fork();
  if (pid_a == 0) {
    for (int i = 0; i < 10; i++) {
      burn_cpu_ms(150);
      call_syscall_write("[A]");
    }
    call_syscall_write("\n[A] fine\n");
    call_syscall_exit();
  }

  int pid_b = call_syscall_fork();
  if (pid_b == 0) {
    for (int i = 0; i < 10; i++) {
      burn_cpu_ms(150);
      call_syscall_write("[B]");
    }
    call_syscall_write("\n[B] fine\n");
    call_syscall_exit();
  }

  call_syscall_wait(pid_a);
  call_syscall_wait(pid_b);
  call_syscall_write("[TEST RR] completato: verificare l'alternanza dei marcatori\n");
  call_syscall_exit();
}
