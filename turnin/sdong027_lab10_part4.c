/*	Author: sdong027
 *  Partner(s) Name: 
 *	Lab Section:
 *	Assignment: Lab #10  Exercise #4
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

// TIMER
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

// PWM
void set_PWM(double frequency) {
	static double current_frequency;
	if (frequency != current_frequency) {
		if (!frequency) { TCCR3B &= 0x08; }
		else { TCCR3B |= 0x03; }

		if (frequency < 0.954) { OCR3A = 0xFFFF; }
		else if (frequency > 31250) { OCR3A = 0x0000; }
		else { OCR3A = (short)(8000000 / (128 * frequency)) - 1; }
	}
}
void PWM_on() {
	TCCR3A = (1 << COM3A0);
	TCCR3B = (1 << WGM32) | (1 << CS31) | (1 << CS30);
	set_PWM(0);
}
void PWM_off() {
	TCCR3A = 0x00;
	TCCR3B = 0x00;
}

// KEYPAD
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

// STATE_FUNCTIONS
unsigned char lockflag = 0;
unsigned char modFlag = 0;
enum ButtonStates {WAIT, LOCK_WAIT_RELEASE, MOD_PASS_WAIT, MOD_PASS};
int LockButton(int state) {			// will detect lock button press and if '*' press
	unsigned char input = ~PINB & 0x80;
	unsigned char keyIn = GetKeypadKey();
	switch (state) {
		case WAIT:
			if (input) {
				state = LOCK_WAIT_RELEASE;
			}
			break;
		case LOCK_WAIT_RELEASE:
			if (!input) {			// only update lockflag (lock door) if it's released... otherwise, it might be a pass change
				state = WAIT;
				lockflag = 1;		
			}
			else if ((keyIn == '*') && input) {
				state = MOD_PASS_WAIT;
			}
			break;
		case MOD_PASS_WAIT:
			if ((keyIn == '\0') && !input) {
				state = MOD_PASS;
				modFlag = 1;
			}
			break;
		case MOD_PASS:
			if (!modFlag) {
				state = WAIT;	// returns but does not change LED state
			}
			break;
		default:
			state = WAIT;
			lockflag = 0;
			modFlag = 0;
			break;
	}
	return state;
}

enum LockStates {LOCK, LOCK_RELEASE, WAIT_PRESS, WAIT_RELEASE, UNLOCK};
unsigned char passcode[5] = {'1','2','3','4','5'};
unsigned short numCodes = 5;
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
				if (i >= numCodes) {
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

enum DB_Button_States {WAITDB, WAIT_DBRELEASE};
unsigned char DBTrigger = 0;
int DoorbellButton(int state) {
	unsigned char input = ~PINA & 0x80;
	switch (state) {
		case WAITDB:
			if (input) {
				state = WAIT_DBRELEASE;
				DBTrigger = 1;			
			}
			break;
		case WAIT_DBRELEASE:
			DBTrigger = 0;
			if (!input) {
				state = WAIT;
			}
			break;
		default:
			state = WAIT;
			DBTrigger = 0;
			break;			
	}
	return state;
}
enum DB_Melody_States {WAIT_PLAY, PLAY_MELODY};
const double melody[] = {261.63, 0.00, 261.63, 0.00, 392.00, 0.00, 392.00, 0.00, 440.00, 0.00, 440.00, 0.00, 392.00, 0.00, 349.23, 0.00, 349.23, 0.00, 329.63, 0.00, 329.63, 0.00, 293.66, 0.00, 293.66, 0.00,  261.63};
int Doorbell(int state) {
	static short i;
	switch (state) {
		case WAIT_PLAY:
			if (DBTrigger) {
				state = PLAY_MELODY;
				PWM_on();
			}
			break;
		case PLAY_MELODY:
			if (i >= 27) {
				state = WAIT_PLAY;
				PWM_off();
				i = 0;
			}
			break;
		default:
			state = WAIT_PLAY;
			i = 0;
			break;			
	}

	switch (state) {
		case WAIT_PLAY:
			PWM_off();
			break;
		case PLAY_MELODY:
			set_PWM(melody[i]);
			i++;
			break;		
	}
	return state;
}

enum ModifyPassStates {WAIT_CHANGE, CHANGE, CHANGE_RELEASE, VERIFY, VERIFY_RELEASE, SET};
unsigned char newPasscode[4] = {0, 0, 0, 0};
int ModifyPass(int state) {
	unsigned char input = GetKeypadKey();
	static unsigned short i;
	switch (state) {
		case WAIT_CHANGE:
			if (modFlag) {
				state = CHANGE;
			}
			i = 0;
			break;
		case CHANGE:					// inputs password
			if (i >= 4) {
				state = VERIFY;
				i = 0;
			}
			else if (input != '\0') {
				state = CHANGE_RELEASE;
				newPasscode[i] = input;
			}
			break;
		case CHANGE_RELEASE:
			if (input == '\0') {
				i++;
				state = CHANGE;		
			}	
			break;
		case VERIFY:					// verifies password
			if (input == newPasscode[i]) {
				state = VERIFY_RELEASE;
				i++;
				if (i >= 4) {
					state = SET;
				}
			}
			else if (input == '\0') {
				state = VERIFY;
			}
			else {					// wrong must restart process
				state = WAIT_CHANGE;
				modFlag = 0;
				i = 0;
			}
			break;
		case VERIFY_RELEASE:
			if (input == '\0') {
				state = VERIFY;
			}
			break;
		case SET:
			modFlag = 0;
			state = WAIT_CHANGE;
			break;
		default:
			state = WAIT_CHANGE;
			i = 0;
			break;
	}

	switch (state) {
		case SET:
			for (unsigned short k = 0; k < 4; k++) {		// updates with new password
				passcode[k] = newPasscode[k];
			}
			numCodes = 4;
			break;
	}

	return state;
}

int main(void) {
	static task task1, task2, task3, task4, task5;
	task *tasks[] = {&task1, &task2, &task3, &task4, &task5};
	const unsigned short numTasks = sizeof(tasks)/sizeof(task*);

	DDRB = 0x00; PORTA = 0xFF;
	DDRB = 0x7F; PORTB = 0x80;
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

	task3.state = start;
	task3.period = 200;
	task3.elapsedTime = task3.period;
	task3.TickFct = &DoorbellButton;

	task4.state = start;
	task4.period = 125;
	task4.elapsedTime = task4.period;
	task4.TickFct = &Doorbell;

	task5.state = start;
	task5.period = 50;
	task5.elapsedTime = task5.period;
	task5.TickFct = &ModifyPass;

	TimerSet(25);
	TimerOn();
	
	while (1) {
		for (unsigned short i = 0; i < numTasks; i++) {
			if (tasks[i]->elapsedTime == tasks[i]->period) {
				tasks[i]->state = tasks[i]->TickFct(tasks[i]->state);
				tasks[i]->elapsedTime = 0;
			}
			tasks[i]->elapsedTime += 25;
		}
		while (!TimerFlag);
		TimerFlag = 0;
	}
}
