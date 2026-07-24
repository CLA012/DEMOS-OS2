#include "../common/user_syscalls.h"
#include "../common/testlib.h"
#include <stddef.h>

// Lottery test (run with active_algorithm = &sched_lottery).
// Three children with 10, 20 and 40 tickets count work iterations within the
// same 3-second window: the expected CPU share is proportional to the tickets
// (1 : 2 : 4), so the counts must grow with the tickets.
// The check is statistical: it verifies the ordering, not the exact ratio
// (the printed values let you appreciate the proportions).
void main() {
  int parent_pid = call_syscall_get_pid();
  int pids[3];

  for (int i = 0; i < 3; i++) {
    int my_id = i + 1;
    int pid = call_syscall_fork();
    if (pid == 0) {
      char go[MAX_MESSAGES_BODY_SIZE];
      // Barrier: everybody starts together, with tickets already assigned
      call_syscall_receive_message(go);
      unsigned long start = call_syscall_get_time();
      unsigned long count = 0;
      volatile unsigned long sink = 0;
      while ((unsigned long)(call_syscall_get_time() - start) < 3000000) {
        for (int k = 0; k < 1000; k++) sink += k;
        count++;
      }
      // The message body is "<id>:<count>"
      char body[32];
      body[0] = (char)('0' + my_id);
      body[1] = ':';
      num_to_str(count, body + 2);
      call_syscall_send_message(parent_pid, body);
      call_syscall_exit();
    }
    pids[i] = pid;
    // Tickets 10, 20, 40: expected shares 1/7, 2/7, 4/7
    call_syscall_set_sched_param(pid, SCHED_PARAM_TICKETS, my_id == 3 ? 40 : my_id * 10);
  }

  call_syscall_write("[TEST LOTTERY] biglietti 10/20/40, finestra di 3 secondi\n");
  for (int i = 0; i < 3; i++) call_syscall_send_message(pids[i], "GO");

  unsigned long counts[4] = {0, 0, 0, 0};
  for (int i = 0; i < 3; i++) {
    char body[MAX_MESSAGES_BODY_SIZE];
    call_syscall_receive_message(body);
    int id = body[0] - '0';
    counts[id] = str_to_num(body + 2);
  }

  for (int id = 1; id <= 3; id++) {
    call_syscall_write("[TEST LOTTERY] figlio ");
    print_num(id);
    call_syscall_write(" (biglietti ");
    print_num(id == 3 ? 40 : id * 10);
    call_syscall_write("): ");
    print_num(counts[id]);
    call_syscall_write(" iterazioni\n");
  }

  if (counts[3] > counts[2] && counts[2] > counts[1])
    call_syscall_write("[TEST LOTTERY] PASS: i conteggi crescono con i biglietti\n");
  else
    call_syscall_write("[TEST LOTTERY] FAIL: conteggi non ordinati con i biglietti\n");

  for (int i = 0; i < 3; i++) call_syscall_wait(pids[i]);
  call_syscall_exit();
}
