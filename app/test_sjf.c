#include "../common/user_syscalls.h"
#include "../common/testlib.h"
#include <stddef.h>

// Test SJF (eseguire con active_algorithm = &sched_sjf).
// Due figli con burst di CPU molto diversi (20 ms vs 300 ms) separati da
// blocchi in ricezione: ad ogni blocco la stima est_burst viene aggiornata
// con la media esponenziale (tau = 0.5*t + 0.5*tau), quindi dopo i primi
// round lo scheduler HA IMPARATO le durate. A ogni via simultaneo del padre,
// il figlio col burst corto deve essere servito per primo: il suo DONE deve
// arrivare prima di quello del burst lungo.
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
        // Il blocco in ricezione chiude il burst e aggiorna est_burst
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
    // Via simultaneo: entrambi i figli diventano pronti insieme
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
