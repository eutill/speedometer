//uncomment the desired target. Also choose corresponding Makefile!
#define ATMEGA328P
//#define ATTINY45

#ifdef ATMEGA328P
#define F_CPU 16000000UL
#define SER 1 //PD
#define RCLK 0 //PD
#define SRCLK 4 //PD
#define DIODE 2 //PD, INT0
#define PUSHBUTTON 5 //PD, PCINT21

#elif defined ATTINY45
#define F_CPU 8000000UL
#define SER 0 //PB
#define RCLK 4 //PB
#define SRCLK 3 //PB
#define DIODE 2 //PB, INT0
#define PUSHBUTTON 1 //PB, PCINT1
#endif

#include <avr/io.h>
#include <stdio.h>
#include <avr/interrupt.h>
#include <util/delay.h>

uint8_t digits[] = {0b00111111, 0b00000110, 0b01011011, 0b01001111, 0b01100110, 0b01101101, 0b01111101, 0b00000111, 0b01111111, 0b01101111};
uint8_t seq[] = {0b01101101, 0b01111001, 0b10111111};
uint8_t max[] = {0b00110111, 0b01110111, 0b01110110};
uint8_t min[] = {0b00110111, 0b00010000, 0b01010100};
uint8_t avg[] = {0b01110111, 0b00011100, 0b00111101};
uint8_t dashes[] = {0b01000000, 0b01000000, 0b01000000};

volatile uint16_t timerOverflow = 0;
volatile uint8_t timerDone = 0;
volatile uint8_t measurementInProgress = 0;

typedef void (*startStopTimer_t) (void);
volatile startStopTimer_t startOrStopTimer;

volatile uint8_t buttonPushed = 0;

void configureTimer(void) {
	TCCR0A = 0x00; //normal operation, timer counts up to MAX
#ifdef ATMEGA328P
	TIMSK0 = 0x01; //enable overflow interrupt
#elif defined ATTINY45
	TIMSK = 0x02; //enable overflow interrupt
#endif
}

void stopTimer(void) { //is also being run by INT0 ISR
	TCCR0B = 0x00; //stop timer
#ifdef ATMEGA328P
	EIMSK = 0x00; //disable INT0 interrupts
	PCICR = 0x00; //disable PC interrupts
#elif defined ATTINY45
	GIMSK = 0x00; //disable INT0 and PC interrupts
#endif
	measurementInProgress = 0;
	timerDone = 1;
}

void startTimer(void) { //is being run by INT0 ISR
#ifdef ATMEGA328P
	TCCR0B = 0x03; //start timer with prescaler 64
	EICRA = 0x02; //interrupt on falling edge
#elif defined ATTINY45
	TCCR0B = 0x02; //start timer with prescaler 8
	MCUCR = (MCUCR & ~(0x03)) | 0x02; //change the values of the bits 1 and 0 of MCUCR to 0b10, interrupt on falling edge
#endif
	startOrStopTimer = stopTimer; //adjust function pointer
	measurementInProgress = 1;
}

void enableExtInt(void) {
	startOrStopTimer = startTimer;
	TCNT0 = 0x00; //reset Timer0 register
	timerOverflow = 0;
#ifdef ATMEGA328P
	PCIFR |= (1<<PCIF2); //clear pending interrupt flags, if any
	PCMSK2 = (1<<PCINT21); //set mask for PCINT21
	PCICR = (1<<PCIE2); //enable pin change interrupt PCI2
	EIFR |= (1<<INTF0); //clear pending interrupt flags, if any
	EICRA = 0x03; //external interrupt on rising edge
	EIMSK = 0x01; //enable external interrupt on INT0
#elif defined ATTINY45
	GIFR  = (1 << INTF0) | (1 << PCIF); // clear pending interrupt flags
	PCMSK = (1 << PCINT1); //enables pin change interrupt on PCINT0
	MCUCR = (MCUCR & ~(0x03)) | 0x03; //external interrupt on rising edge
	GIMSK = (1 << INT0) | (1 << PCIE); //enable external interrupt on INT0 and pin change interrupt
#endif
	timerDone = 0; //falls nicht zuvor gelöscht
	buttonPushed = 0;
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

void threeCharPrint(uint8_t *string) {
	for(int8_t i=2;i>=0;i--) { //Last char first
		shiftOut(*(string+i));
	}
	display();
}

void printInteger(uint16_t number) {
	if(number > 999) {
		number = 999;
	}
	char numberString[4];
	snprintf(numberString, 4, "%3u", number);
	for(int8_t i=2;i>=0;i--) { //Last char first
		if(numberString[i] == ' ') {
			numberString[i] = 0b00;
		} else {
			numberString[i] -= '0';
			numberString[i] = digits[(uint8_t) numberString[i]];
		}
		shiftOut(numberString[i]);
	}
	display();
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
	microsString[3] |= (1<<7); //decimal point for seconds
	microsString[6] |= (1<<7); //decimal point for milliseconds
	unsigned long boundaries[] = {10000,100000,1000000,10000000,100000000,1000000000};
	for(int j=0;j<6;j++) {
		if(timeMicros < boundaries[j]) {
			for(int b=0;b<3;b++) {
				char symbol = microsString[8-j-b];
				if(b==0) { //Last digit is transmitted first
					if(j>2) { //if displayed value is in seconds, the last digit will have an appended point
						symbol |= (1 << 7);
					} else { //if displayed value is in millisecs, no appended point (also not a decimal point)
						symbol &= ~(1 << 7);
					}
				}
				shiftOut(symbol);
			}
			display();
			break;
		}
	}

	/*
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
	}*/
}

unsigned long calcTime() {
	unsigned long time;
#ifdef ATMEGA328P
	time = (unsigned long)timerOverflow * 1024;
	time += (unsigned long) 4 * TCNT0;
#elif defined ATTINY45
	time = (unsigned long)timerOverflow * 256;
        time += TCNT0;
#endif
	return time;
}

uint8_t takeMeasurement(unsigned long *duration) {//takes a measurement and displays it. Interrupts measurement if button is pressed. Returns 1 if measurement completed, otherwise 0. If address != NULL is passed as argument, measured time is stored there.
	enableExtInt();
	unsigned long time;
	while(1) {
		if(measurementInProgress) {
			time = calcTime();
			printTimeMicros(time);
		} else if(timerDone) {
			time = calcTime();
			printTimeMicros(time);
			timerDone = 0;
			if(duration != NULL) {
				*duration = time;
			}
			return 1;
		}
		if(buttonPushed) {
			buttonPushed = 0;
			stopTimer();
			return 0;
		}
	}
}

int main(void) {
#ifdef ATMEGA328P
        DDRD |= _BV(SER) | _BV(RCLK) | _BV(SRCLK); //set these pins as outputs
	PORTD |= _BV(PUSHBUTTON); //enable pull-up
#elif defined ATTINY45
        DDRB |= _BV(SER) | _BV(RCLK) | _BV(SRCLK); //set these pins as outputs
	PORTB |= _BV(PUSHBUTTON); //enable pull-up
#endif

	configureTimer();
	sei();

	for(int i=3;i>0;i--) {
        	shiftOut(0xff);
        }
	display();
	_delay_ms(1000);

	uint16_t n;
	unsigned long minVal;
	unsigned long maxVal;
	unsigned long avgSum;
	unsigned long measurement;
	uint8_t *titleList[] = {avg, min, max};
	unsigned long *valueList[] = {&avgSum, &minVal, &maxVal};
	while(1) {
		printTimeMicros(0);
		while(1) { //Normal (single-shot) operation
			if(!takeMeasurement(NULL)) {//button pressed
				break;
			}
		}

		n = 0;
		avgSum = 0;
		maxVal = 0;
		minVal = 0-1;

		//Sequence operation
		threeCharPrint(seq);
		_delay_ms(1000);
		while(1) {
			printInteger(n);
			if(!takeMeasurement(&measurement)) {break;} //button pressed
			avgSum += measurement;
			if(measurement > maxVal) {maxVal = measurement;}
			if(measurement < minVal) {minVal = measurement;}
			n++;
			_delay_ms(1500);
		}
		if(n > 0) {
			avgSum /= n;
			for(int i=0;i<3;i++) {
				threeCharPrint(titleList[i]);
				_delay_ms(2000);
				printTimeMicros(*valueList[i]);
#ifdef ATMEGA328P
			while((PIND & (1 << PUSHBUTTON))); //active LOW
#elif defined ATTINY45
			while((PINB & (1 << PUSHBUTTON)));
#endif
			}
		}
		threeCharPrint(dashes);
		_delay_ms(1000);
	}

	return 0;
}

ISR(TIMER0_OVF_vect) {
	timerOverflow++;
}

ISR(INT0_vect) {
	startOrStopTimer();
}

#ifdef ATMEGA328P
ISR(PCINT2_vect) {
#elif defined ATTINY45
ISR(PCINT0_vect) {
#endif
	buttonPushed = 1;
}
