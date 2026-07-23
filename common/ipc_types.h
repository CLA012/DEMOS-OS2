#ifndef __IPC_TYPES_H
#define __IPC_TYPES_H

/**
 * ipc_types.h defines the constants used to make inter process communication possible
 */

 // Signals flags
#define SIGNALS_NUMBER 3
#define SIGNAL_KILL 1
#define SIGNAL_STOP 2
#define SIGNAL_RESUME 3

// Maximum size in bytes of a message body: shared between the kernel (PCB
// message buffer, see scheduler.h) and the user programs that receive messages
#define MAX_MESSAGES_BODY_SIZE 256

#endif
