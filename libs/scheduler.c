// Include the header file containing scheduler definitions and macros
#include "scheduler.h"
// Include the header for interrupt request (IRQ) controller management
#include "../drivers/irq/controller.h"
// Include the header for memory management functions and structures
#include "mm.h"
// Include the header for process creation (fork) functionalities
#include "fork.h"

// Initialize the init process (the idle task) using a predefined macro
static struct PCB init_process = INIT_PROCESS;
// Set the pointer for the currently running process to the init process
struct PCB *current_process = &init_process;

// Global array holding all processes, initialized with the init process at index 0
struct PCB *processes[N_PROCESSES] = { &init_process };
// Counter for the total number of active processes (starts at 1 because of init)
int n_processes = 1; // n_processes = 0 ==> &init_process

// =========================================================================
// DATA STRUCTURES FOR QUEUES (THEORY)
// =========================================================================
// Define a structure to represent a queue of processes
typedef struct {
    // Pointer to the first process (PCB) in the queue
    struct PCB* head;
    // Pointer to the last process (PCB) in the queue
    struct PCB* tail;
} ProcessQueue;

// Macro to initialize an empty queue with NULL head and tail pointers
#define QUEUE_INIT {NULL, NULL}

// Define the ready queue used by standard algorithms like FCFS, RR, SJF, LJF
static ProcessQueue ready_queue = QUEUE_INIT;                              
// Define an array of 2 queues for Multilevel Queue (MLQ: 0=Foreground, 1=Background)
static ProcessQueue mlq_queues[2] = {QUEUE_INIT, QUEUE_INIT};              
// Define an array of 3 queues for Multilevel Feedback Queue (MLFQ)
static ProcessQueue mlfq_queues[3] = {QUEUE_INIT, QUEUE_INIT, QUEUE_INIT};
// Array to track the current MLFQ queue level (priority) for each process by its PID
static int queue_level[N_PROCESSES] = {0};

// =========================================================================
// ACTIVE ALGORITHM SELECTION (single configuration point)
// =========================================================================
// To change the scheduling algorithm, point active_algorithm to one of the
// descriptors declared in scheduler.h and defined at the bottom of this file:
// sched_round_robin, sched_fcfs, sched_sjf, sched_priority_aging, sched_mlq,
// sched_mlfq. Nothing else needs to be touched: insertion policy, selection
// policy, per-tick bookkeeping and preemptiveness are all part of the descriptor
static const SchedAlgorithm* active_algorithm = &sched_round_robin;

// =========================================================================
// QUEUE OPERATIONS (O(1) Complexity)
// =========================================================================
// Function to add a process to the end of a specific queue
void enqueue_process_to(ProcessQueue* queue, struct PCB* process) {
    // If the process is NULL or is the idle init process, do not enqueue it
    if (!process || process == &init_process) return;
    // Ensure the process's next pointer is clear before adding it
    process->next_ready = NULL;
    // A process sitting in a ready queue is READY: it will become RUNNING only
    // when switch_to_process assigns it the CPU
    process->state = PROCESS_READY;

    // If the queue is currently empty
    if (queue->tail == NULL) {
        // The new process becomes both the head of the queue...
        queue->head = process;
        // ...and the tail of the queue
        queue->tail = process;
    } else {
        // Otherwise, link the current tail's next pointer to the new process
        queue->tail->next_ready = process;
        // Update the queue's tail pointer to be the new process
        queue->tail = process;
    }
}

// Function to remove and return the process at the front of a specific queue
struct PCB* dequeue_process_from(ProcessQueue* queue) {
    // If the queue is empty (head is NULL), return NULL
    if (queue->head == NULL) return NULL;
    
    // Store the process currently at the head of the queue
    struct PCB* process = queue->head;
    // Move the head pointer to the next process in line
    queue->head = process->next_ready;
    
    // If the queue is now empty after removing the head
    if (queue->head == NULL) {
        // Set the tail pointer to NULL as well
        queue->tail = NULL;
    }
    
    // Disconnect the removed process from the queue by clearing its next pointer
    process->next_ready = NULL;
    // Return the extracted process
    return process;
}

// Function to insert a process into a queue kept sorted by the estimated length
// of the next CPU burst (est_burst): ascending order for SJF, descending for LJF
void enqueue_sorted_to(ProcessQueue* queue, struct PCB* process, int ascending) {
    // Ignore NULL processes or the init process
    if (!process || process == &init_process) return;
    // Clear the process's next pointer
    process->next_ready = NULL;
    // A process sitting in a ready queue is READY (see enqueue_process_to)
    process->state = PROCESS_READY;

    // If the queue is empty, the new process becomes both head and tail (this
    // case must be handled before dereferencing queue->head for the comparison)
    if (queue->head == NULL) {
        queue->head = process;
        queue->tail = process;
        return;
    }

    // Determine if the new process should be placed at the very front of the queue
    // based on whether we are sorting ascending (SJF) or descending (LJF)
    int goes_first = ascending ? (process->est_burst < queue->head->est_burst)
                               : (process->est_burst > queue->head->est_burst);

    // If the new process has the shortest (SJF) / longest (LJF) estimated burst
    if (goes_first) {
        // Point the new process's next to the current head
        process->next_ready = queue->head;
        // Update the queue's head to be the new process
        queue->head = process;
        // Exit the function since insertion is complete
        return;
    }

    // Initialize a pointer to traverse the queue starting from the head
    struct PCB* current = queue->head;
    // Variable to hold the loop condition evaluation
    int condition;
    // Traverse the queue to find the correct insertion spot
    while (current->next_ready != NULL) {
        // Check if the new process estimate fits the sorted order compared to the next node
        condition = ascending ? (process->est_burst >= current->next_ready->est_burst)
                              : (process->est_burst <= current->next_ready->est_burst);
        // If the condition is false, we have found the insertion point
        if (!condition) break;
        // Move to the next process in the queue
        current = current->next_ready;
    }

    // Link the new process to the node that will follow it
    process->next_ready = current->next_ready;
    // Link the current node to the new process
    current->next_ready = process;
    // If we inserted at the very end of the queue
    if (process->next_ready == NULL) {
        // Update the queue's tail pointer to the new process
        queue->tail = process;
    }
}

// =========================================================================
// INSERTION POLICIES (the enqueue callback of each algorithm descriptor)
// =========================================================================

// --- Round Robin / FCFS: plain FIFO insertion at the tail of the ready queue ---
static void _enqueue_fifo(struct PCB* process) {
    // Enqueue the process at the end of the standard ready queue
    enqueue_process_to(&ready_queue, process);
}

// --- SJF (Shortest Job First): insertion sorted by shortest job (ascending order) ---
static void _enqueue_sjf(struct PCB* process) {
    // Insert the process keeping the queue ordered by the estimated length of
    // the next CPU burst (est_burst), shortest at the head
    enqueue_sorted_to(&ready_queue, process, 1);
}

// --- Priority aging: NO enqueueing ---
// The priority aging algorithm does not use ready queues at all:
// _schedule_priority_aging scans the whole processes[] array at every decision,
// so a process that becomes ready does not need to be inserted anywhere.
// This explicitly empty callback documents that choice
static void _enqueue_priority_aging(struct PCB* process) {
    // No queue insertion: marking the process READY is enough to make it
    // selectable by the scan in _schedule_priority_aging
    process->state = PROCESS_READY;
}

// --- MLQ (Multilevel Queue): fixed queue chosen by the process's class ---
// The class is a PCB field set at process creation (see fork.c), replacing the
// old PID-parity criterion that gave the user no control over the priority of
// the processes it creates (PIDs are assigned automatically)
static void _enqueue_mlq(struct PCB* process) {
    // Foreground processes go into the high priority queue (index 0)
    if (process->queue_class == QUEUE_CLASS_FOREGROUND) enqueue_process_to(&mlq_queues[0], process);
    // Background processes go into the low priority queue (index 1)
    else enqueue_process_to(&mlq_queues[1], process);
}

// --- MLFQ (Multilevel Feedback Queue): queue matching the process's current level ---
static void _enqueue_mlfq(struct PCB* process) {
    // Enqueue the process into the specific MLFQ queue corresponding to its current priority level
    enqueue_process_to(&mlfq_queues[queue_level[process->pid]], process);
}

// =========================================================================
// INSERTION ROUTER (Called by fork.c, syscalls, and signal handlers)
// =========================================================================
// Centralized function to enqueue a process that became ready: it simply
// delegates to the insertion policy of the active algorithm
void enqueue_process(struct PCB* process) {
    // Ignore NULL processes or the init process
    if (!process || process == &init_process) return;

    // Delegate to the active algorithm's insertion policy
    active_algorithm->enqueue(process);
}

// =========================================================================
// SYSTEM MANAGEMENT FUNCTIONS
// =========================================================================
// Function to register a newly created process into the scheduler's tracking system
int add_process_to_scheduler(struct PCB* process) {
    // Return error (-1) if we reached max capacity or if the PID is out of bounds
    if(n_processes >= N_PROCESSES || process->pid >= N_PROCESSES) return -1;
    
    // Store the process pointer in the global processes array at the index of its PID
    processes[process->pid] = process;
    // MLFQ Rule: Every new process starts at the highest priority queue (Level 0)
    queue_level[process->pid] = 0; 

    // If the process is initialized in a READY (runnable) state...
    if (process->state == PROCESS_READY) {
        // ...enqueue it into the ready queues using the routing function
        enqueue_process(process);
    }
    
    // Increment the total count of active processes
    n_processes++;
    // Return 0 on success
    return 0;
}

// The scheduler lock is a critical-section guard for the scheduler and its data
// structures: while sched_lock_count is greater than 0 the timer tick will not
// trigger a context switch, so the locked code cannot be interrupted halfway
// through a queue manipulation. Note that this is NOT the preemption policy of
// the scheduling algorithm: that is expressed by the is_preemptive field of the
// SchedAlgorithm descriptor. The counter nests: each sched_lock() must be
// balanced by a sched_unlock()
// Function to release the scheduler lock (allows context switches again)
void sched_unlock() { current_process->sched_lock_count--; }
// Function to acquire the scheduler lock (prevents context switches)
void sched_lock() { current_process->sched_lock_count++; }

// Forward declaration for the signal handling function
void handle_process_signals(struct PCB* process);
// Forward declaration for the context switch setup function
void switch_to_process(struct PCB *next_process);
// External declaration for the assembly routine that actually swaps CPU registers
extern void cpu_switch_to_process(struct PCB *prev, struct PCB *next);

// Priority scheduling algorithm with an aging mechanism to prevent starvation.
// It follows the historic Linux "epoch" scheme, where time_slice plays a double
// role: residual quantum AND dynamic priority. The runnable process with the
// largest time_slice wins the CPU; when all runnable processes have exhausted
// their slice the epoch ends and every process is recharged (see below)
void _schedule_priority_aging() {
  // Take the scheduler lock so the decision cannot be interrupted
  sched_lock();
  // Variables to track the highest residual time slice and the index of the chosen process
  long max_time_slice, next_process_index;

  // Infinite loop to find a process to run
  while (1) {
    // Reset the max time slice for this pass
    max_time_slice = 0;
    // Default to the init process (index 0)
    next_process_index = 0;
    
    // Loop through all possible process slots in the global array
    for (int i = 0; i < N_PROCESSES; i++) {
      // If a process exists at this index
      if (processes[i]) {
        // Process any pending signals (kill, stop, resume) for this process
        handle_process_signals(processes[i]);
        // Both READY processes and the currently RUNNING one compete with their
        // residual time_slice; the idle init process never takes part in the
        // selection (it is only the fallback when nobody else is runnable)
        if (processes[i] != &init_process
            && (processes[i]->state == PROCESS_READY || processes[i]->state == PROCESS_RUNNING)
            && processes[i]->time_slice > max_time_slice) {
          // Update the maximum time slice found so far
          max_time_slice = processes[i]->time_slice;
          // Save the index of this process as the next potential candidate
          next_process_index = i;
        }
      }
    }

    // If we found at least one ready process with a residual time slice greater than 0
    if (max_time_slice > 0) {
      // Break out of the infinite loop
      break;
    }

    // If no process had time_slice > 0 (all runnable ones are exhausted), the
    // epoch is over: we start a new one applying the aging rule
    for (int i = 0; i < N_PROCESSES; i++) {
      // If the process exists
      if (processes[i]) {
        // New epoch: time_slice = time_slice/2 + priority, for ALL processes,
        // blocked ones included. A CPU-bound process that used up its slice simply
        // reloads its static priority; a blocked process keeps half of its residual
        // slice as a bonus (geometric series capped at 2*priority), so I/O-bound
        // processes gain dynamic priority: this is the aging that prevents starvation
        processes[i]->time_slice = (processes[i]->time_slice >> 1) + processes[i]->priority;
      }
    }
  } // End of while(1) loop

  // Retrieve the pointer to the selected next process
  struct PCB* next_process = processes[next_process_index];
  // Handle signals for the selected process one more time just in case
  handle_process_signals(next_process);

  // Verify the process is still runnable (signals might have stopped/killed it)
  if (next_process->state == PROCESS_READY || next_process->state == PROCESS_RUNNING) {
    // Perform the context switch to the selected process
    switch_to_process(next_process);
  }
  // Release the scheduler lock before leaving the scheduler
  sched_unlock();
}

// =========================================================================
// ALGORITHM IMPLEMENTATIONS USING QUEUES
// =========================================================================

// Round Robin scheduling algorithm implementation
void _schedule_round_robin() {
    // Take the scheduler lock during the scheduling decision
    sched_lock();

    // 1. If the current process was interrupted but is still runnable (not init)...
    if (current_process != &init_process && current_process->state == PROCESS_RUNNING) {
        // ...put it back at the end of the ready queue
        enqueue_process_to(&ready_queue, current_process);
    }

    // 2. Initialize pointer for the next process
    struct PCB* next_process = NULL;
    // Extract processes from the queue in O(1) time until we find a valid one
    while ((next_process = dequeue_process_from(&ready_queue)) != NULL) {
        // Process any pending signals for the extracted process
        handle_process_signals(next_process);
        // If the process is still READY after handling signals, break the loop
        if (next_process->state == PROCESS_READY) break;
    }

    // If the queue was empty or all processes were stopped/zombies
    if (next_process == NULL) next_process = &init_process; // Fallback to idle task
    // Otherwise, recharge its time quantum (e.g., 10 ticks)
    else next_process->time_slice = 10;

    // Dispatch the selected process (switch_to_process marks it RUNNING and does
    // nothing more if it is already the current one)
    switch_to_process(next_process);
    // Release the scheduler lock
    sched_unlock();
}

// Shared selection function for the non-preemptive algorithms (FCFS, SJF):
// their difference lives entirely in the enqueue callback (FIFO vs. sorted), so
// picking always means "take the head of the ready queue". These algorithms have
// is_preemptive = 0 in their descriptor, therefore the timer tick never calls
// this function: it only runs when the current process blocks, yields or exits.
// This replaces the two identical _schedule_fcfs/_schedule_sjf functions and
// their misleading early-return on a still-running current process
void _schedule_queue_head() {
    // Take the scheduler lock during the scheduling decision
    sched_lock();

    // If the current process yielded voluntarily but is still runnable, put it
    // back in the ready queue according to the algorithm's insertion policy
    if (current_process != &init_process && current_process->state == PROCESS_RUNNING) {
        active_algorithm->enqueue(current_process);
    }

    // Initialize pointer for the next process
    struct PCB* next_process = NULL;
    // Extract processes from the front of the queue
    while ((next_process = dequeue_process_from(&ready_queue)) != NULL) {
        // Handle pending signals
        handle_process_signals(next_process);
        // If it's still READY, break the loop and choose it
        if (next_process->state == PROCESS_READY) break;
    }

    // If no valid process was found, fallback to the init process
    if (next_process == NULL) next_process = &init_process;

    // Dispatch the selected process (switch_to_process marks it RUNNING and does
    // nothing more if it is already the current one)
    switch_to_process(next_process);
    // Release the scheduler lock
    sched_unlock();
}

// Multilevel Queue scheduling algorithm implementation
void _schedule_mlq() {
    // Take the scheduler lock during the scheduling decision
    sched_lock();

    // Re-insert the preempted process into the fixed queue of its class
    if (current_process != &init_process && current_process->state == PROCESS_RUNNING) {
        _enqueue_mlq(current_process);
    }

    // Initialize pointer for the next process
    struct PCB* next_process = NULL;

    // Search in the Foreground queue first (Highest Priority)
    while ((next_process = dequeue_process_from(&mlq_queues[0])) != NULL) {
        // Handle pending signals
        handle_process_signals(next_process);
        // If still READY
        if (next_process->state == PROCESS_READY) {
            // Assign a time quantum of 10 ticks
            next_process->time_slice = 10;
            // Break loop as we found a process
            break;
        }
    }

    // If the Foreground queue was empty, search in the Background queue (Low Priority)
    if (next_process == NULL) {
        // Dequeue from Background queue
        while ((next_process = dequeue_process_from(&mlq_queues[1])) != NULL) {
            // Handle pending signals
            handle_process_signals(next_process);
            // If still READY
            if (next_process->state == PROCESS_READY) {
                // Assign a time quantum of 20 ticks: background (batch/CPU-bound) processes
                // get a longer slice than foreground ones to reduce context switches.
                // Without this the time slice stayed at 0, so after the first expiry a
                // background process ran with a de facto quantum of a single tick
                next_process->time_slice = 20;
                // Break loop as we found a process
                break;
            }
        }
    }

    // Fallback to init process if both queues are empty
    if (next_process == NULL) next_process = &init_process;

    // Dispatch the selected process (switch_to_process marks it RUNNING and does
    // nothing more if it is already the current one)
    switch_to_process(next_process);
    // Release the scheduler lock
    sched_unlock();
}

// Multilevel Feedback Queue scheduling algorithm implementation
void _schedule_mlfq() {
    // Take the scheduler lock during the scheduling decision
    sched_lock();

    // If the process yielded voluntarily (e.g., for I/O) and still has time left (time_slice > 0)
    // we do not demote it. We put it back in its current priority queue.
    if (current_process != &init_process && current_process->state == PROCESS_RUNNING && current_process->time_slice > 0) {
         // Re-insert into the queue matching its current tracked level
         enqueue_process_to(&mlfq_queues[queue_level[current_process->pid]], current_process);
    }

    // Initialize pointer for the next process
    struct PCB* next_process = NULL;
    // Variable to track which queue the process was extracted from
    int target_queue = -1;

    // O(1) Cascade: iterate through queues starting from 0 (Highest Priority) to 2 (Lowest)
    for (int q = 0; q < 3; q++) {
        // Dequeue processes from the current queue level
        while ((next_process = dequeue_process_from(&mlfq_queues[q])) != NULL) {
            // Handle pending signals
            handle_process_signals(next_process);
            // If the process is still READY
            if (next_process->state == PROCESS_READY) {
                // Record the queue level it came from
                target_queue = q;
                // Break out of the while loop
                break;
            }
        }
        // If we found a process in this queue level, break the outer for-loop (do not check lower priority queues)
        if (target_queue != -1) break; 
    }

    // If all queues were completely empty
    if (next_process == NULL) {
        // Set the next process to the idle task
        next_process = &init_process;
    } else {
        // Assign differentiated time quanta based on the priority queue level
        // Highest priority gets the shortest time slice
        if (target_queue == 0) next_process->time_slice = 5;
        // Medium priority gets a medium time slice
        else if (target_queue == 1) next_process->time_slice = 10;
        // Lowest priority gets the longest time slice (best for CPU-bound tasks)
        else next_process->time_slice = 20;
    }

    // Dispatch the selected process (switch_to_process marks it RUNNING and does
    // nothing more if it is already the current one)
    switch_to_process(next_process);
    // Release the scheduler lock
    sched_unlock();
}

// =========================================================================
// MLFQ PER-TICK BOOKKEEPING (the on_tick callback of sched_mlfq)
// =========================================================================
// Static variable to count hardware timer ticks for the MLFQ boost mechanism
static int mlfq_ticks_since_boost = 0;

// Runs at every timer tick, but only when MLFQ is the active algorithm: it is
// registered as the on_tick callback of the sched_mlfq descriptor, so no other
// algorithm ever executes it (the queues are intrusive lists sharing the PCB's
// next_ready pointer: feeding mlfq_queues while e.g. Round Robin uses
// ready_queue would corrupt both lists)
static void _mlfq_on_tick() {
    // Increment the counter tracking time since the last MLFQ priority boost
    mlfq_ticks_since_boost++;
    // Anti-Starvation Rule: If 1000 ticks have passed
    if (mlfq_ticks_since_boost >= 1000) {
        // Physically empty the lower priority queues (1 and 2) and move everyone to queue 0
        for (int q = 1; q < 3; q++) {
            struct PCB* p;
            // Dequeue until empty
            while ((p = dequeue_process_from(&mlfq_queues[q])) != NULL) {
                // Reset their tracked priority level to 0
                queue_level[p->pid] = 0;
                // Enqueue them into the highest priority queue
                enqueue_process_to(&mlfq_queues[0], p);
            }
        }

        // Also reset the priority level tracking array for all processes (even blocked ones)
        for (int i = 0; i < N_PROCESSES; i++) {
            queue_level[i] = 0;
        }
        // Reset the boost timer
        mlfq_ticks_since_boost = 0;
    }

    // Penalty (Demotion): if the current process has just exhausted its time slice
    // without blocking, it is CPU-bound and gets demoted. The check on
    // sched_lock_count mirrors the one in handle_timer_tick: while the scheduler
    // is locked the process will keep running, so it must not be parked in a queue
    if (current_process != &init_process && current_process->time_slice <= 0 && current_process->sched_lock_count == 0) {
        // If it's not already in the lowest priority queue (Level 2)
        if (queue_level[current_process->pid] < 2) {
            // Demote it to the next lower queue level
            queue_level[current_process->pid]++;
        }
        // Physically park the process in its new (or same, if already at bottom) queue
        enqueue_process_to(&mlfq_queues[queue_level[current_process->pid]], current_process);
    }
}

// =========================================================================
// ALGORITHM DESCRIPTORS
// =========================================================================
// --- Round Robin: FIFO ready queue, preemptive with fixed time quantum ---
const SchedAlgorithm sched_round_robin = {
    .enqueue = _enqueue_fifo,
    .pick_next = _schedule_round_robin,
    .on_tick = NULL,
    .is_preemptive = 1,
};

// --- FCFS: FIFO ready queue, non-preemptive (runs until block/yield/exit) ---
const SchedAlgorithm sched_fcfs = {
    .enqueue = _enqueue_fifo,
    .pick_next = _schedule_queue_head,
    .on_tick = NULL,
    .is_preemptive = 0,
};

// --- SJF: ready queue sorted by shortest job, non-preemptive ---
const SchedAlgorithm sched_sjf = {
    .enqueue = _enqueue_sjf,
    .pick_next = _schedule_queue_head,
    .on_tick = NULL,
    .is_preemptive = 0,
};

// --- Priority with aging: no queues, scans processes[], preemptive ---
const SchedAlgorithm sched_priority_aging = {
    .enqueue = _enqueue_priority_aging,
    .pick_next = _schedule_priority_aging,
    .on_tick = NULL,
    .is_preemptive = 1,
};

// --- MLQ: two fixed-priority queues (foreground/background), preemptive ---
const SchedAlgorithm sched_mlq = {
    .enqueue = _enqueue_mlq,
    .pick_next = _schedule_mlq,
    .on_tick = NULL,
    .is_preemptive = 1,
};

// --- MLFQ: three feedback queues with demotion and periodic boost, preemptive ---
const SchedAlgorithm sched_mlfq = {
    .enqueue = _enqueue_mlfq,
    .pick_next = _schedule_mlfq,
    .on_tick = _mlfq_on_tick,
    .is_preemptive = 1,
};

// =========================================================================
// CENTRAL DISPATCHER
// =========================================================================
// Master scheduling function: delegates the decision to the active algorithm
// (see the ACTIVE ALGORITHM SELECTION section at the top of this file)
void _schedule() {
    active_algorithm->pick_next();
}

// Wrapper function to trigger a manual schedule (e.g., when a process yields or blocks)
void schedule() {
    // If the process is giving up the CPU because it blocked (or terminated), its
    // CPU burst is over: update the estimate of the next burst with an exponential
    // average, est_burst = alpha * measured + (1 - alpha) * est_burst, alpha = 0.5.
    // A voluntary yield (state still RUNNING) does not end the burst
    if (current_process != &init_process
        && current_process->state != PROCESS_RUNNING
        && current_process->state != PROCESS_READY) {
        current_process->est_burst = (current_process->burst_ticks + current_process->est_burst) / 2;
        // Reset the measurement for the next burst
        current_process->burst_ticks = 0;
    }
    // Force the current process's time slice to 0 so it gets preempted/re-evaluated
    current_process->time_slice = 0;
    // Call the master dispatcher
    _schedule();
}

// =========================================================================
// SIGNALS AND CONTEXT MANAGEMENT
// =========================================================================
// Function to evaluate and apply pending signals for a specific process
void handle_process_signals(struct PCB* process) {
    // If the bitmask for pending signals is 0, exit immediately (nothing to do)
    if (!process->pending_signals) return;

    // Check if the SIGNAL_KILL bit is set
    if (process->pending_signals & (1 << SIGNAL_KILL)) {
        // Change process state to ZOMBIE (terminated but waiting for parent to read status)
        process->state = PROCESS_ZOMBIE;
        // Clear the SIGNAL_KILL bit from the pending signals mask
        process->pending_signals &= ~(1 << SIGNAL_KILL);
        
    // Check if the SIGNAL_STOP bit is set
    } else if (process->pending_signals & (1 << SIGNAL_STOP)) {
        // Change process state to STOPPED (suspended execution)
        process->state = PROCESS_STOPPED;
        // Clear the SIGNAL_STOP bit from the pending signals mask
        process->pending_signals &= ~(1 << SIGNAL_STOP);
        
    // Check if the SIGNAL_RESUME bit is set
    } else if (process->pending_signals & (1 << SIGNAL_RESUME)) {
        // Clear the SIGNAL_RESUME bit from the pending signals mask
        process->pending_signals &= ~(1 << SIGNAL_RESUME);
        // A resume only makes sense for a STOPPED process: re-enqueueing a process
        // that is already READY in a queue would link it twice into the intrusive
        // lists, corrupting them
        if (process->state == PROCESS_STOPPED) {
            // Change process state back to READY (runnable, waiting for the CPU)
            process->state = PROCESS_READY;
            // Wake up the process by re-inserting it into the ready queues
            enqueue_process(process);
        }
    }
}

// Function to handle the high-level context switch preparation
void switch_to_process(struct PCB *next_process) {
    // The dispatched process becomes the (only) RUNNING one. This must happen even
    // when the scheduler re-selects the current process, which may just have been
    // marked READY by a re-enqueue in the pick function
    next_process->state = PROCESS_RUNNING;
    // If the selected process is already running, no context switch is needed
    if (current_process == next_process) return;
    // Save the pointer to the currently running process
    struct PCB *previous_process = current_process;
    // If the outgoing process is still runnable (preempted, not blocked) and was
    // not already re-enqueued by the pick function, it goes back to READY
    if (previous_process->state == PROCESS_RUNNING) previous_process->state = PROCESS_READY;
    // Update the global current_process pointer to the new process
    current_process = next_process;

    // Update the Memory Management Unit's Page Global Directory to the new process's address space
    set_pgd(next_process->mm.pgd);
    // Call the assembly routine to swap CPU registers and stack pointers
    cpu_switch_to_process(previous_process, current_process);
}

// Function called at the end of a newly created process's first context switch to
// release the scheduler lock it was created with (see copy_process)
void schedule_tail(void) { sched_unlock(); }

// =========================================================================
// TIMER TICK
// =========================================================================
// Function called by the hardware timer interrupt handler at regular intervals
void handle_timer_tick() {
    // Decrement the residual time slice of the currently running process
    current_process->time_slice -= 1;
    // Account the tick to the CPU burst the current process is consuming: the
    // measurement feeds the est_burst estimate used by SJF/LJF (see schedule)
    current_process->burst_ticks += 1;

    // Run the active algorithm's per-tick bookkeeping, if it has any
    // (currently only MLFQ does: periodic boost and demotions)
    if (active_algorithm->on_tick) active_algorithm->on_tick();

    // If the current process still has time left OR it holds the scheduler lock
    if (current_process->time_slice > 0 || current_process->sched_lock_count > 0) {
        // Return immediately, allowing the process to continue running
        return;
    }

    // Safety catch: ensure the time slice doesn't go negative
    current_process->time_slice = 0;

    // Non-preemptive algorithms (FCFS, SJF) never switch on a timer tick: the
    // current process keeps the CPU until it blocks, yields or exits, so the
    // dispatcher must not even be called here
    if (!active_algorithm->is_preemptive) return;

    // Re-enable hardware interrupts before calling the scheduler
    enable_irq();
    // Call the master dispatcher to choose a new process
    _schedule();
    // Disable hardware interrupts upon returning from the scheduler
    disable_irq();
}

// =========================================================================
// EXIT AND SYSCALL WAIT MANAGEMENT
// =========================================================================
// Function called when a process terminates
void exit_process() {
    // Take the scheduler lock to ensure atomicity of the exit process
    sched_lock();
    // Set the current process's state to ZOMBIE
    current_process->state = PROCESS_ZOMBIE;
    
    // Scan the entire process array to find if any parent process is waiting for this child
    for (int i = 0; i < N_PROCESSES; i++) {
        // Skip empty slots
        if (!processes[i]) continue;

        // If a process is blocked (WAITING) and specifically waiting for the exiting process's PID
        if (processes[i]->state == PROCESS_WAITING_ANOTHER_PROCESS && processes[i]->pid_to_wait == current_process->pid) {
            // Wake up the waiting parent process (READY: it still has to be dispatched)
            processes[i]->state = PROCESS_READY;
            // Clear the wait condition
            processes[i]->pid_to_wait = -1;
            
            // Re-enqueue the freshly woken parent into the ready queues
            enqueue_process(processes[i]);
        }
    }

    // Release the scheduler lock
    sched_unlock();
    // Call the scheduler to switch away from the dying process permanently
    schedule();
}
