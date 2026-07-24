#include "../common/user_syscalls.h"
#include "../common/testlib.h"
#include <stddef.h>

// SJF test (run with active_algorithm = &sched_sjf).
// Two children with very different CPU bursts (20 ms vs 300 ms) separated by
// blocking receives: at every block the est_burst estimate is updated with
// the exponential average (tau = 0.5*t + 0.5*tau), so after the first rounds
// the scheduler HAS LEARNED the durations. At every simultaneous go from the
// parent, the child with the short burst must be served first: its DONE must
// arrive before the long burst's one.
#define ROUNDS 4

void main() {
  int parent_pid = call_syscall_get_pid();
  int pids[2];

  call_syscall_write("[TEST SJF] burst 20ms vs 300ms: dopo il warmup vince il corto\n");

  for (int i = 0; i < 2; i++) {
    int my_id = i + 1;
    unsigned long my_burst = (i == 0) ? 20 : 300;
    int pid = call_syscall_fork();
    if (pid == 0) {
      for (int r = 0; r < ROUNDS; r++) {
        char go[MAX_MESSAGES_BODY_SIZE];
        // The blocking receive closes the burst and updates est_burst
        call_syscall_receive_message(go);
        burn_cpu_ms(my_burst);
        char body[24];
        num_to_str(my_id, body);
        call_syscall_send_message(parent_pid, body);
      }
      call_syscall_exit();
    }
    pids[i] = pid;
  }

  int last_first = 0;
  for (int r = 0; r < ROUNDS; r++) {
    // Simultaneous go: both children become ready together
    call_syscall_send_message(pids[0], "GO");
    call_syscall_send_message(pids[1], "GO");

    char body[MAX_MESSAGES_BODY_SIZE];
    call_syscall_receive_message(body);
    last_first = (int)str_to_num(body);
    call_syscall_write("[TEST SJF] round ");
    print_num(r + 1);
    call_syscall_write(": primo DONE dal figlio ");
    print_num(last_first);
    if (last_first == 1) call_syscall_write(" (burst corto)\n");
    else call_syscall_write(" (burst lungo)\n");
    call_syscall_receive_message(body);
  }

  if (last_first == 1) call_syscall_write("[TEST SJF] PASS: il burst corto e' servito per primo\n");
  else call_syscall_write("[TEST SJF] FAIL: servito prima il burst lungo\n");

  for (int i = 0; i < 2; i++) call_syscall_wait(pids[i]);
  call_syscall_exit();
}
