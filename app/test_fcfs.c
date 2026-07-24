#include "../common/user_syscalls.h"
#include "../common/testlib.h"
#include <stddef.h>

// FCFS test (run with active_algorithm = &sched_fcfs).
// Three CPU-bound children created in the order 1, 2, 3: since the algorithm
// is non-preemptive, each one must run TO COMPLETION before the next, and the
// completion order must match the creation order.
// Each child notifies its id to the parent via IPC when its work is done:
// the parent checks that the ids arrive in the order 1, 2, 3.
#define N_CHILDREN 3

void main() {
  int parent_pid = call_syscall_get_pid();
  int pids[N_CHILDREN];

  call_syscall_write("[TEST FCFS] 3 figli CPU-bound creati in ordine 1, 2, 3\n");

  for (int i = 0; i < N_CHILDREN; i++) {
    // The child's identity is copied by the fork together with the memory
    int my_id = i + 1;
    int pid = call_syscall_fork();
    if (pid == 0) {
      call_syscall_write("[FIGLIO ");
      print_num(my_id);
      call_syscall_write("] START\n");
      burn_cpu_ms(300);
      char body[24];
      num_to_str(my_id, body);
      call_syscall_send_message(parent_pid, body);
      call_syscall_write("[FIGLIO ");
      print_num(my_id);
      call_syscall_write("] END\n");
      call_syscall_exit();
    }
    pids[i] = pid;
  }

  // The parent receives the ENDs: they must arrive in creation order
  int pass = 1;
  for (int i = 0; i < N_CHILDREN; i++) {
    char body[MAX_MESSAGES_BODY_SIZE];
    call_syscall_receive_message(body);
    unsigned long finished = str_to_num(body);
    call_syscall_write("[TEST FCFS] completato il figlio ");
    print_num(finished);
    call_syscall_write("\n");
    if (finished != (unsigned long)(i + 1)) pass = 0;
  }

  for (int i = 0; i < N_CHILDREN; i++) call_syscall_wait(pids[i]);

  if (pass) call_syscall_write("[TEST FCFS] PASS: completamento nell'ordine di creazione\n");
  else call_syscall_write("[TEST FCFS] FAIL: ordine diverso da quello di creazione\n");
  call_syscall_exit();
}
