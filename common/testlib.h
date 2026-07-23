#ifndef __TESTLIB_H
#define __TESTLIB_H

/**
 * testlib.h defines the small helpers shared by the scheduler test programs
 * (app/test_*.c): decimal printing/parsing and a calibrated CPU burner.
 */

// Writes the decimal representation of the number into the buffer
// (the buffer must be at least 24 bytes long)
void num_to_str(unsigned long number, char* buffer);
// Prints the number in decimal on the UART (via the write syscall)
void print_num(unsigned long number);
// Parses a decimal number from the beginning of the string
unsigned long str_to_num(char* buffer);
// Burns CPU for approximately the given milliseconds, measured on the system
// timer (get_time syscall): it never blocks, so it is a pure CPU burst
void burn_cpu_ms(unsigned long milliseconds);

#endif
