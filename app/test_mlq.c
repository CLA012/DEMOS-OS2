#include "../common/user_syscalls.h"
#include "../common/testlib.h"
#include <stddef.h>

// MLQ test (run with active_algorithm = &sched_mlq).
// Two identical CPU-bound children in queues of different priority: level 0
// (highest) and level 3. With FIXED priorities, level 3 runs only when the
// level-0 queue is empty: the level-0 child must complete first, even though
// the level-3 one was created and woken up before it.
// The kernel [MLFQ/MLQ] traces show every process in the queue of its own
// priority.
void main() {
  int parent_pid = call_syscall_get_pid();
  int pids[2];

  for (int i = 0; i < 2; i++) {
    // The first created child goes to level 3, the second to level 0: if the
    // creation order won (and not the priority) the test would fail
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
  // The level-3 child receives the go first: it will still start second
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
