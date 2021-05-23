/*	Author: sdong027
 *  Partner(s) Name: 
 *	Lab Section:
 *	Assignment: Lab #10  Exercise #2
 *	Exercise Description: [optional - include for your own benefit]
 *
 *	I acknowledge all content contained herein, excluding template or example
 *	code, is my own original work.
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#ifdef _SIMULATE_
#include "simAVRHeader.h"
#endif
#include "bit.h"
#include "scheduler.h"

volatile unsigned char TimerFlag = 0;
unsigned long _avr_timer_M = 1;
unsigned long _avr_timer_cntcurr = 0;

void TimerSet(unsigned long M) {
	_avr_timer_M = M;
	_avr_timer_cntcurr = _avr_timer_M;
}
void TimerOn() {
	TCCR1B = 0x0B;
	OCR1A = 125;
	TIMSK1 = 0x02;
	TCNT1 = 0;
	_avr_timer_cntcurr = _avr_timer_M;
	SREG |= 0x80;
}
void TimerOff() {
	TCCR1B = 0x00;
}
void TimerISR() {
	TimerFlag = 1;
}
ISR(TIMER1_COMPA_vect) {
	_avr_timer_cntcurr--;
	if (_avr_timer_cntcurr == 0) {
		TimerISR();
		_avr_timer_cntcurr = _avr_timer_M;
	}
}

unsigned char GetKeypadKey() {
	PORTC = 0xEF;	// First column
	asm("nop");
	if (GetBit(PINC, 0) == 0) { return('1'); }
	if (GetBit(PINC, 1) == 0) { return('4'); }
	if (GetBit(PINC, 2) == 0) { return('7'); }
	if (GetBit(PINC, 3) == 0) { return('*'); }

	PORTC = 0xDF;	// Second Column
	asm("nop");
	if (GetBit(PINC, 0) == 0) { return('2'); }
	if (GetBit(PINC, 1) == 0) { return('5'); }
	if (GetBit(PINC, 2) == 0) { return('8'); }
	if (GetBit(PINC, 3) == 0) { return('0'); }
	// finish

	PORTC = 0xBF;	// Third Column
	asm("nop");
	if (GetBit(PINC, 0) == 0) { return('3'); }
	if (GetBit(PINC, 1) == 0) { return('6'); }
	if (GetBit(PINC, 2) == 0) { return('9'); }
	if (GetBit(PINC, 3) == 0) { return('#'); }
	// finish

	PORTC = 0x7F;	// Fourth column
	asm("nop");
	if (GetBit(PINC, 0) == 0) { return('A'); }
	if (GetBit(PINC, 1) == 0) { return('B'); }
	if (GetBit(PINC, 2) == 0) { return('C'); }
	if (GetBit(PINC, 3) == 0) { return('D'); }

	return('\0');
}

unsigned char lockflag = 0;
enum ButtonStates {WAIT, LOCK_WAIT_RELEASE} B_STATES;
int LockButton(int state) {
	unsigned char input = ~PINB & 0x80;
	switch (state) {
		case WAIT:
			if (input) {
				state = LOCK_WAIT_RELEASE;
				lockflag = 1;			
			}
			break;
		case LOCK_WAIT_RELEASE:
			if (!input) {
				state = WAIT;
			}
			break;
		default:
			state = WAIT;
			lockflag = 0;
			break;
	}
	return state;
}

enum LockStates {LOCK, LOCK_RELEASE, WAIT_PRESS, WAIT_RELEASE, UNLOCK} LED_STATES;
unsigned char passcode[5] = {'1','2','3','4','5'};
int LockOut(int state) {
	static unsigned short i;
	unsigned char in = GetKeypadKey();
	switch (state) {
		case LOCK:
			if (in == '#' && !(lockflag)) {		// if # pressed, begin sequence
				state = LOCK_RELEASE;
			}
			else {					// wrong input or PB7 pressed, restart
				i = 0;
			}
			
			break;
		case LOCK_RELEASE:
			if (in == '\0' && !(lockflag)) {
				state = WAIT_PRESS;
			}
			else if (lockflag) {
				state = LOCK;
			}
			break;
		case WAIT_PRESS:
			if (in == passcode[i] && !(lockflag)) {	// if a correct character is pressed, continue to next character
				state = WAIT_RELEASE;
				i++;
				if (i >= 5) {
					state = UNLOCK;
				}
			}
			else if (in == '#' && !(lockflag)) {
				state = LOCK_RELEASE;
				i = 0;
			}
			else if (in == '\0' && !(lockflag)) {	// if no character pressed (and PB7 not pressed), continue waiting
				state = WAIT_PRESS;
			}
			else {					// wrong input or PB7 pressed, restart
				state = LOCK;
				i = 0;
			}
			break;
		case WAIT_RELEASE:
			if (in == '\0' && !(lockflag)) {
				state = WAIT_PRESS;
			}
			else if (lockflag) {
				state = LOCK;
			}
			break;
		case UNLOCK:
			if (in == '\0' && lockflag) {
				state = LOCK;			
			}
			break;
		default: 
			state = LOCK; 
			i = 0;			
			break;
	}

	switch (state) {
		case LOCK:
			PORTB = 0x00;
			lockflag = 0;
			break;
		case UNLOCK:
			PORTB = 0x01;
			break;
	}

	return state;
}

int main(void) {
	unsigned char x;
	static task task1, task2;
	task *tasks[] = {&task1, &task2};
	const unsigned short numTasks = sizeof(tasks)/sizeof(task*);

	DDRB = 0x0F; PORTB = 0xF0;
	DDRC = 0xF0; PORTC = 0x0F;

	const char start = -1;
	task1.state = start;
	task1.period = 50;
	task1.elapsedTime = task1.period;
	task1.TickFct = &LockButton;

	task2.state = start;
	task2.period = 50;
	task2.elapsedTime = task2.period;
	task2.TickFct = &LockOut;

	TimerSet(50);
	TimerOn();
	
	while (1) {
		for (unsigned short i = 0; i < numTasks; i++) {
			if (tasks[i]->elapsedTime == tasks[i]->period) {
				tasks[i]->state = tasks[i]->TickFct(tasks[i]->state);
				tasks[i]->elapsedTime = 0;
			}
			tasks[i]->elapsedTime += 50;
		}
		while (!TimerFlag);
		TimerFlag = 0;
	}
}
