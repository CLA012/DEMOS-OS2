/**
 * Assembly utility functions.
 */

.globl get_el
get_el:
	// Returns the current exception level (EL)
	mrs x0, CurrentEL
	lsr x0, x0, #2
	ret

.globl delay
delay:
	// Busy-wait decrementing a counter
	subs x0, x0, #1
	bne	delay
	ret

