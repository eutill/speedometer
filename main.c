//uncomment the desired target. Also choose corresponding Makefile!
#define ATMEGA328P
//#define ATTINY45

#ifdef ATMEGA328P
#define F_CPU 16000000UL
#define SER 1 //PD
#define RCLK 0 //PD
#define SRCLK 4 //PD
#define DIODE INT0 //PD2

#elif defined ATTINY45
#define F_CPU 8000000UL
#define SER 1 //PB
#define RCLK 3 //PB
#define SRCLK 4 //PB
#define DIODE INT0 //PB2
//Instead of connecting to INT0, diode can be connected to AC inputs: reference voltage and signal on PB0, PB1
#endif

#include <avr/io.h>
#include <stdio.h>
#include <avr/interrupt.h>
#include <util/delay.h>

uint8_t digits[] = {0b00111111, 0b00000110, 0b01011011, 0b01001111, 0b01100110, 0b01101101, 0b01111101, 0b00000111, 0b01111111, 0b01101111};
volatile uint16_t timerOverflow = 0;
volatile uint8_t timerDone = 0;
volatile uint8_t measurementInProgress = 0;

typedef void (*startStopTimer_t) (void);
volatile startStopTimer_t startOrStopTimer;

void prepareTimer(void) {
	TCCR0B = 0x00; //stop timer if still running
	TCNT0 = 0x00; //reset timer register
	TCCR0A = 0x00; //normal operation, timer counts up to MAX
#ifdef ATMEGA328P
	TIMSK0 = 0x01; //enable overflow interrupt
#elif defined ATTINY45
	TIMSK = 0x02; //enable overflow interrupt
#endif
	timerOverflow = 0;
	//don't enable global interrupts just yet
}

void stopTimer(void) {
	TCCR0B = 0x0;
	cli();
	timerDone = 1;
	measurementInProgress = 0;
}

void startTimer(void) {
#ifdef ATMEGA328P
	TCCR0B = 0x03; //start timer with prescaler 64
	EICRA = 0x2; //interrupt on falling edge
#elif defined ATTINY45
	TCCR0B = 0x02; //start timer with prescaler 8
	MCUCR = (MCUCR & ~(0x03)) | 0x02; //change the values of the bits 1 and 0 of MCUCR to 0b10, interrupt on falling edge
#endif
	startOrStopTimer = stopTimer; //adjust function pointer
	measurementInProgress = 1;
}

void enableExtInt(void) {
	prepareTimer();
#ifdef ATMEGA328P
	EICRA = 0x03; //external interrupt on rising edge
	EIMSK = 0x01; //enable external interrupt on INT0
#elif defined ATTINY45
	MCUCR = (MCUCR & ~(0x03)) | 0x03; //interrupt on rising edge
	GIMSK = (1 << 6); //enable external interrupt on INT0
#endif
	startOrStopTimer = startTimer;
	sei();
}

void shiftOut(uint8_t data) {
	uint8_t bit;
	for(int i=7;i>=0;i--) { //MSB first
		bit = (data >> i) & (uint8_t) 1;
#ifdef ATMEGA328P
		if(bit == 1) {
			PORTD |= _BV(SER);
		} else {
			PORTD &= ~(_BV(SER));
		}
		_delay_ms(1);
		PORTD |= _BV(SRCLK);
		_delay_ms(1);
		PORTD &= ~(_BV(SRCLK));
		_delay_ms(1);
#elif defined ATTINY45
		if(bit == 1) {
                        PORTB |= _BV(SER);
                } else {
                        PORTB &= ~(_BV(SER));
                }
                _delay_ms(1);
                PORTB |= _BV(SRCLK);
                _delay_ms(1);
                PORTB &= ~(_BV(SRCLK));
                _delay_ms(1);
#endif
	}
}

void display(void) {
#ifdef ATMEGA328P
	PORTD |= _BV(RCLK);
	_delay_ms(1);
	PORTD &= ~(_BV(RCLK));
#elif defined ATTINY45
	PORTB |= _BV(RCLK);
        _delay_ms(1);
        PORTB &= ~(_BV(RCLK));
#endif
}

void printTimeMicros(unsigned long timeMicros) {
	if(timeMicros >= 1000000000) {
		timeMicros = 999999999;
	}
	//TODO: round value to required display precision
	char microsString[11];
	snprintf(microsString, 11, "%.10lu", timeMicros);
	for(int i=0;i<10;i++) { //replace each ASCII digit with its segment representation
		microsString[i] -= '0';
		microsString[i] = digits[(uint8_t) microsString[i]];
	}
	if(timeMicros < 10000) {
		shiftOut(microsString[8]);
		shiftOut(microsString[7]);
		shiftOut(microsString[6] | (1<<7)); //digit + decimal point
	} else if(timeMicros < 100000) {
		shiftOut(microsString[7]);
		shiftOut(microsString[6] | (1<<7));
		shiftOut(microsString[5]);
	} else if(timeMicros < 1000000) {
		shiftOut(microsString[6]);
		shiftOut(microsString[5]);
		shiftOut(microsString[4]);
	} else if(timeMicros < 10000000) {
		shiftOut(microsString[5] | (1 << 7));
		shiftOut(microsString[4]);
		shiftOut(microsString[3] | (1 << 7));
	} else if(timeMicros < 100000000) {
		shiftOut(microsString[4] | (1 << 7));
		shiftOut(microsString[3] | (1 << 7));
		shiftOut(microsString[2]);
	} else {
		shiftOut(microsString[3] | (1 << 7));
		shiftOut(microsString[2]);
		shiftOut(microsString[1]);
	}
	display();
}

void calcTime(void) {
#ifdef ATMEGA328P
	unsigned long time = (unsigned long)timerOverflow * 1024;
	time += (unsigned long) 4 * TCNT0;
#elif defined ATTINY45
	unsigned long time = (unsigned long)timerOverflow * 256;
        time += TCNT0;
#endif
	printTimeMicros(time);
}

int main(void) {
#ifdef ATMEGA328P
        DDRD |= _BV(SER) | _BV(RCLK) | _BV(SRCLK);
#elif defined ATTINY45
        DDRB |= _BV(SER) | _BV(RCLK) | _BV(SRCLK);
#endif

	prepareTimer();

	for(int i=3;i>0;i--) {
        	shiftOut(0xff);
        }
	display();
	_delay_ms(1000);

	printTimeMicros(0);
	enableExtInt();
	while(1) {
		while(measurementInProgress) {
			calcTime();
		}
		if(timerDone) {
			calcTime();
			enableExtInt();
			timerDone = 0;
		}
	}

	return 0;
}

ISR(TIMER0_OVF_vect) {
	timerOverflow++;
}

ISR(INT0_vect) {
	startOrStopTimer();
}
