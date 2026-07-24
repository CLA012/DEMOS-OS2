#include "../common/user_syscalls.h"
#include "../common/testlib.h"
#include <stddef.h>

// Priority aging test (run with active_algorithm = &sched_priority_aging).
// Two IDENTICAL CPU-bound children (2 seconds of work), but with static
// priorities 1 and 4: the epoch recharge counter = counter/2 + priority gives
// the second one about 4 times more CPU, so it must complete first.
// Anti-starvation: the priority-1 child still makes progress and completes.
// The children start together thanks to an IPC barrier: first they block in
// receive, the parent sets the priorities and then sends the go to both.
void main() {
  int parent_pid = call_syscall_get_pid();
  int pids[2];

  for (int i = 0; i < 2; i++) {
    int my_prio = (i == 0) ? 1 : 4;
    int pid = call_syscall_fork();
    if (pid == 0) {
      char go[MAX_MESSAGES_BODY_SIZE];
      // Barrier: the child waits for the go, so the parent has the time to
      // set the priority before the work begins
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
