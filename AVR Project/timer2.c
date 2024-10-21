/*
 * timer2.c
 *
 * Author: Peter Sutton
 */

#include "timer2.h"
#include <avr/io.h>
#include <avr/interrupt.h>
uint8_t seven_seg[10] = { 63,6,91,79,102,109,125,7,127,111};
volatile uint8_t step_count = 0;
volatile uint8_t seven_seg_cc = 0; // 0 = right, 1 = left

void init_timer2(void)
{
	// Setup timer 2.
	TCNT2 = 0;
	DDRA = 0xFF;
	DDRC = 0x01;
	increment_step_count();
	
}
void increment_step_count(void) {
	step_count = (step_count + 1) % 100;
}

// Display the current step count on the seven-segment display
void display_step_count(void) {
	if (seven_seg_cc == 0) {
		// Display right digit (units place)
		PORTA = seven_seg[step_count % 10];
		} else {
		// Display left digit (tens place)
		uint8_t left_digit = step_count / 10;
		PORTA = (left_digit == 0) ? 0 : seven_seg[left_digit];  // Blank or 0 for single digits
	}
	// Output the digit selection bit
	PORTC = seven_seg_cc;
}

// Interrupt Service Routine (ISR) to switch between digits
ISR(TIMER2_OVF_vect) {
	seven_seg_cc = 1 ^ seven_seg_cc;  // Toggle between 0 and 1
	display_step_count();             // Update the display
}