#include "../common/user_syscalls.h"
#include "../common/testlib.h"
#include <stddef.h>

// Test MLQ (eseguire con active_algorithm = &sched_mlq).
// Due figli CPU-bound identici in code di priorita' diverse: livello 0
// (massima) e livello 3. A priorita' FISSE, il livello 3 gira solo quando la
// coda di livello 0 e' vuota: il figlio a livello 0 deve completare per primo,
// anche se quello a livello 3 e' stato creato e svegliato prima di lui.
// Le tracce [MLFQ/MLQ] del kernel mostrano ogni processo nella coda della
// sua priorita'.
void main() {
  int parent_pid = call_syscall_get_pid();
  int pids[2];

  for (int i = 0; i < 2; i++) {
    // Il primo figlio creato va al livello 3, il secondo al livello 0: se
    // vincesse l'ordine di creazione (e non la priorita') il test fallirebbe
    int my_level = (i == 0) ? 3 : 0;
    int pid = call_syscall_fork();
    if (pid == 0) {
      char go[MAX_MESSAGES_BODY_SIZE];
      call_syscall_receive_message(go);
      call_syscall_write("[FIGLIO L");
      print_num(my_level);
      call_syscall_write("] START\n");
      burn_cpu_ms(1000);
      char body[24];
      num_to_str(my_level, body);
      call_syscall_send_message(parent_pid, body);
      call_syscall_exit();
    }
    pids[i] = pid;
    call_syscall_set_sched_param(pid, SCHED_PARAM_QUEUE_PRIORITY, my_level);
  }

  call_syscall_write("[TEST MLQ] livello 0 contro livello 3 (creato prima il 3)\n");
  // Il figlio a livello 3 riceve il via per primo: partira' comunque secondo
  call_syscall_send_message(pids[0], "GO");
  call_syscall_send_message(pids[1], "GO");

  char body[MAX_MESSAGES_BODY_SIZE];
  call_syscall_receive_message(body);
  unsigned long first = str_to_num(body);
  call_syscall_write("[TEST MLQ] primo a completare: livello ");
  print_num(first);
  call_syscall_write("\n");
  call_syscall_receive_message(body);

  if (first == 0) call_syscall_write("[TEST MLQ] PASS: la coda a priorita' piu' alta vince\n");
  else call_syscall_write("[TEST MLQ] FAIL: ha completato prima il livello 3\n");

  for (int i = 0; i < 2; i++) call_syscall_wait(pids[i]);
  call_syscall_exit();
}
