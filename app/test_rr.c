#include "../common/user_syscalls.h"
#include "../common/testlib.h"
#include <stddef.h>

// Test Round Robin (eseguire con active_algorithm = &sched_round_robin).
// Due figli CPU-bound stampano un marcatore ogni ~150 ms di CPU consumata:
// con il quanto di 10 tick (100 ms) la preemption li alterna, quindi i
// marcatori [A] e [B] devono risultare mescolati. Per confronto, con FCFS
// attivo uscirebbero prima tutti gli [A] e poi tutti i [B].
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
