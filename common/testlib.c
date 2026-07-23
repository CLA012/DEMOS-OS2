#include "testlib.h"
#include "user_syscalls.h"

// Writes the decimal representation of the number into the buffer
void num_to_str(unsigned long number, char* buffer) {
  char reversed[24];
  int i = 0;

  // Zero is a special case: no digits would be produced by the loop
  if (number == 0) {
    buffer[0] = '0';
    buffer[1] = '\0';
    return;
  }

  // Extract the digits from the least significant one
  while (number > 0) {
    reversed[i] = '0' + (number % 10);
    number /= 10;
    i++;
  }

  // Reverse them into the output buffer
  int j = 0;
  while (i > 0) {
    i--;
    buffer[j] = reversed[i];
    j++;
  }
  buffer[j] = '\0';
}

// Prints the number in decimal on the UART
void print_num(unsigned long number) {
  char buffer[24];
  num_to_str(number, buffer);
  call_syscall_write(buffer);
}

// Parses a decimal number from the beginning of the string
unsigned long str_to_num(char* buffer) {
  unsigned long number = 0;
  while (*buffer >= '0' && *buffer <= '9') {
    number = number * 10 + (unsigned long)(*buffer - '0');
    buffer++;
  }
  return number;
}

// Burns CPU for approximately the given milliseconds. The elapsed time is
// measured with the get_time syscall (microseconds of the free-running system
// timer): the loop never blocks, so for the scheduler this is one long CPU
// burst that consumes full time slices
void burn_cpu_ms(unsigned long milliseconds) {
  unsigned long start = call_syscall_get_time();
  while ((unsigned long)(call_syscall_get_time() - start) < milliseconds * 1000) {
    // Busy wait: the work is the loop itself
  }
}
