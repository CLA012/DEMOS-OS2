#ifndef _SCHEDULER_H
#define _SCHEDULER_H

#define CPU_CONTEXT_OFFSET_IN_PCB 0

#ifndef __ASSEMBLER__

#define N_PROCESSES 64

#define THREAD_SIZE 4096

#define MAX_FILES_PER_PROCESS 16
#define MAX_PROCESS_PAGES 16
#define MAX_MESSAGES_PER_PROCESS 4
#define MAX_MESSAGES_BODY_SIZE 256

#define PF_KTHREAD 0x00000002

struct cpu_context {
  unsigned long x19;
  unsigned long x20;
  unsigned long x21;
  unsigned long x22;
  unsigned long x23;
  unsigned long x24;
  unsigned long x25;
  unsigned long x26;
  unsigned long x27;
  unsigned long x28;
  unsigned long fp;
  unsigned long sp;
  unsigned long pc;
};

#include "./fat32/fat.h"
#include "../common/ipc_types.h"

typedef enum { RESOURCE_TYPE_FILE, RESOURCE_TYPE_FOLDER } ResourceType;

typedef struct {
  ResourceType resource_type;

  union {
    File *f;
    Dir *d;
  };
} FatResource;

struct user_page {
    unsigned long physical_address;
    unsigned long virtual_address;
};

struct mm_struct {
    unsigned long pgd;
    int n_user_pages;
    struct user_page user_pages[MAX_PROCESS_PAGES];
    int n_kernel_pages;
    unsigned long kernel_pages[MAX_PROCESS_PAGES];
};

struct Message {
    struct PCB* source_process;
    struct PCB* destination_process;
    char body[MAX_MESSAGES_BODY_SIZE];
};

struct MessagesCircularBuffer {
  volatile int head;
  volatile int tail;
  struct Message buffer[MAX_MESSAGES_PER_PROCESS];
};

struct PCB {
  struct cpu_context cpu_context;
  long state;
  // Residual time slice in timer ticks: recharged by the scheduling algorithm
  // when the process is dispatched and decremented at every timer tick; when it
  // reaches 0 the process can be preempted. For the priority aging algorithm it
  // doubles as the DYNAMIC priority of the process (historic Linux epoch scheme,
  // see _schedule_priority_aging in scheduler.c)
  long time_slice;
  // Static priority: used by the priority aging algorithm to recharge time_slice
  // at the beginning of every epoch
  long priority;
  // Scheduler lock nesting counter: while greater than 0 the timer tick will not
  // preempt this process (critical-section guard, see sched_lock/sched_unlock).
  // Not to be confused with the algorithm's preemption policy (is_preemptive)
  int sched_lock_count;
  long pid;

  unsigned long flags;

  FatResource *files[MAX_FILES_PER_PROCESS];

  struct mm_struct mm;

  struct MessagesCircularBuffer messages_buffer;

  int pending_signals;

  int pid_to_wait;
  struct PCB* next_ready;
};

// The process currently owning the CPU: at most one process is in this state at
// any time, and the transition to it happens only inside switch_to_process
#define PROCESS_RUNNING 1
#define PROCESS_ZOMBIE 2
#define PROCESS_WAITING_UART_INPUT 3
#define PROCESS_WAITING_TO_RECEIVE_MESSAGE 4
#define PROCESS_WAITING_TO_SEND_MESSAGE 5
#define PROCESS_WAITING_ANOTHER_PROCESS 6
#define PROCESS_STOPPED 7
// The process is runnable but does NOT have the CPU: it sits in a ready queue
// (or, for priority aging, simply in processes[]) waiting to be dispatched.
// It goes back to this state every time it is re-enqueued
#define PROCESS_READY 8

#define INIT_PROCESS {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 0, 1, 1, 0, 0, 0, {}, {0, 0, {}, 0, {}}, {}, 0, -1, NULL}

// =========================================================================
// SCHEDULING ALGORITHM DESCRIPTOR
// =========================================================================
// Every scheduling algorithm is described by a table of function pointers, so that
// all its algorithm-specific behaviour lives behind a single interface.
// Switching algorithm only requires changing which descriptor the pointer
// active_algorithm (in scheduler.c) refers to: insertion policy, selection policy,
// per-tick bookkeeping and preemptiveness all follow automatically.
typedef struct {
  // Inserts a process that became ready into the algorithm's ready structure(s)
  void (*enqueue)(struct PCB* process);
  // Selects the next process to run and performs the context switch
  void (*pick_next)(void);
  // Optional per-tick bookkeeping (e.g. MLFQ promotions/demotions); NULL if unused
  void (*on_tick)(void);
  // If 0 the timer tick never forces a context switch: the running process keeps
  // the CPU until it blocks, yields or exits (FCFS, SJF)
  int is_preemptive;
} SchedAlgorithm;

// Descriptors for all the available scheduling algorithms (defined in scheduler.c)
extern const SchedAlgorithm sched_round_robin;
extern const SchedAlgorithm sched_fcfs;
extern const SchedAlgorithm sched_sjf;
extern const SchedAlgorithm sched_priority_aging;
extern const SchedAlgorithm sched_mlq;
extern const SchedAlgorithm sched_mlfq;

extern struct PCB *current_process;
extern struct PCB *processes[N_PROCESSES];
extern int n_processes;

// Critical-section guard of the scheduler: code between sched_lock() and
// sched_unlock() cannot be preempted by the timer tick. This is a protection
// for the scheduler's data structures, NOT the preemption policy of the
// scheduling algorithm (which is the is_preemptive field of the descriptor)
extern void sched_unlock();
extern void sched_lock();
extern void schedule();
extern void switch_to_process(struct PCB *);
extern void handle_timer_tick();
extern void exit_process();
extern int add_process_to_scheduler(struct PCB* process);
extern void enqueue_process(struct PCB* process);
#endif
#endif
