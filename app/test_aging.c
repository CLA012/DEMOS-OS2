#include "../common/user_syscalls.h"
#include "../common/testlib.h"
#include <stddef.h>

// Test priority aging (eseguire con active_algorithm = &sched_priority_aging).
// Due figli CPU-bound IDENTICI (2 secondi di lavoro), ma con priorita'
// statiche 1 e 4: la ricarica d'epoca counter = counter/2 + priority da' al
// secondo circa 4 volte piu' CPU, quindi deve completare per primo.
// Anti-starvation: anche il figlio a priorita' 1 avanza comunque e completa.
// I figli partono insieme grazie a una barriera IPC: prima si bloccano in
// ricezione, il padre imposta le priorita' e poi manda il via a entrambi.
void main() {
  int parent_pid = call_syscall_get_pid();
  int pids[2];

  for (int i = 0; i < 2; i++) {
    int my_prio = (i == 0) ? 1 : 4;
    int pid = call_syscall_fork();
    if (pid == 0) {
      char go[MAX_MESSAGES_BODY_SIZE];
      // Barriera: il figlio aspetta il via, cosi' il padre fa in tempo a
      // impostare la priorita' prima che il lavoro cominci
      call_syscall_receive_message(go);
      burn_cpu_ms(2000);
      char body[24];
      num_to_str(my_prio, body);
      call_syscall_send_message(parent_pid, body);
      call_syscall_exit();
    }
    pids[i] = pid;
    call_syscall_set_sched_param(pid, SCHED_PARAM_PRIORITY, my_prio);
  }

  call_syscall_write("[TEST AGING] stesso lavoro (2s), priorita' statiche 1 e 4\n");
  call_syscall_send_message(pids[0], "GO");
  call_syscall_send_message(pids[1], "GO");

  char body[MAX_MESSAGES_BODY_SIZE];
  call_syscall_receive_message(body);
  unsigned long first = str_to_num(body);
  call_syscall_write("[TEST AGING] primo a completare: priorita' ");
  print_num(first);
  call_syscall_write("\n");
  call_syscall_receive_message(body);
  call_syscall_write("[TEST AGING] completato anche l'altro (niente starvation)\n");

  if (first == 4) call_syscall_write("[TEST AGING] PASS: ha vinto la priorita' piu' alta\n");
  else call_syscall_write("[TEST AGING] FAIL: atteso il figlio a priorita' 4\n");

  for (int i = 0; i < 2; i++) call_syscall_wait(pids[i]);
  call_syscall_exit();
}
