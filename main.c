//uncomment the desired target. Also choose corresponding Makefile!
//#define ATMEGA328P
//#define ATTINY45
#define ATTINY84A

#ifdef ATMEGA328P
#define F_CPU 16000000UL
#define IODDR DDRD
#define IOPORT PORTD
#define IOPIN PIND
#define SER 1 //PD
#define RCLK 0 //PD
#define SRCLK 4 //PD
#define PUSHBUTTON 5 //PD

#define DIODE 2 //PD, INT0


#elif defined ATTINY45
#define F_CPU 8000000UL
#define IODDR DDRB
#define IOPORT PORTB
#define IOPIN PINB
#define SER 0 //PB
#define RCLK 4 //PB
#define SRCLK 3 //PB
#define PUSHBUTTON 1 //PB

#define DIODE 2 //PB, INT0

#elif defined ATTINY84A
#define F_CPU 8000000UL
#define IODDR DDRA
#define IOPORT PORTA
#define IOPIN PINA
#define SER 2 //PA
#define RCLK 1 //PA
#define SRCLK 0 //PA
#define PUSHBUTTON 3 //PA
#define UARTX 5 //PA

#define DIODE 2 //PB, INT0
#endif

#define SHREG_BACKWARDS

#define UART_PRINT 1
#define UART_NO_PRINT 0

#include <avr/io.h>
#include <stdio.h>
#include <avr/interrupt.h>
#include <util/delay.h>

static uint8_t digits[] = {0b00111111, 0b00000110, 0b01011011, 0b01001111, 0b01100110, 0b01101101, 0b01111101, 0b00000111, 0b01111111, 0b01101111};
static uint8_t seq[] = {0b01101101, 0b01111001, 0b10111111};
static uint8_t max[] = {0b00110111, 0b01110111, 0b01110110};
static uint8_t min[] = {0b00110111, 0b00010000, 0b01010100};
static uint8_t avg[] = {0b01110111, 0b00011100, 0b00111101};
static uint8_t dashes[] = {0b01000000, 0b01000000, 0b01000000};

volatile uint16_t timerOverflow = 0;
volatile uint8_t timerDone = 0;
volatile uint8_t measurementInProgress = 0;

typedef void (*startStopTimer_t) (void);
volatile startStopTimer_t startOrStopTimer;

void configureTimer(void) {
	TCCR0A = 0x00; //normal operation, timer counts up to MAX
#if defined (ATMEGA328P) || defined (ATTINY84A)
	TIMSK0 = 0x01; //enable overflow interrupt
#elif defined ATTINY45
	TIMSK = 0x02; //enable overflow interrupt
#endif
}

void stopTimer(void) { //is also being run by INT0 ISR
	TCCR0B = 0x00; //stop timer
#ifdef ATMEGA328P
	EIMSK = 0x00; //disable INT0 interrupts
#elif defined (ATTINY84A) || defined (ATTINY45)
	GIMSK = 0x00; //disable INT0 interrupts
#endif
	measurementInProgress = 0;
	timerDone = 1;
}

void startTimer(void) { //is being run by INT0 ISR
#ifdef ATMEGA328P
	TCCR0B = 0x03; //start timer with prescaler 64
	EICRA = 0x02; //interrupt on falling edge
#elif defined (ATTINY84A) || defined (ATTINY45)
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
	EIFR |= (1<<INTF0); //clear pending interrupt flag, if any
	EICRA = 0x03; //external interrupt on rising edge
	EIMSK = 0x01; //enable external interrupt on INT0
#elif defined (ATTINY45) || defined (ATTINY84A)
	GIFR  = (1 << INTF0); // clear pending interrupt flag
	MCUCR = MCUCR | 0x03; //external interrupt on rising edge
	GIMSK = (1 << INT0); //enable external interrupt on INT0
#endif
	timerDone = 0; //if not already reset
}

void shiftOut(uint8_t data) {
	uint8_t bit;
	for(int i=7;i>=0;i--) { //MSB first
		bit = (data >> i) & (uint8_t) 1;
		if(bit) {
			IOPORT |= _BV(SER);
		} else {
			IOPORT &= ~(_BV(SER));
		}
		_delay_ms(1);
		IOPORT |= _BV(SRCLK);
		_delay_ms(1);
		IOPORT &= ~(_BV(SRCLK));
		_delay_ms(1);
	}
}

void display(void) {
	IOPORT |= _BV(RCLK);
	_delay_ms(1);
	IOPORT &= ~(_BV(RCLK));
}

void threeCharPrint(uint8_t *string) {
#ifdef SHREG_BACKWARDS
	for(int8_t i=0;i<3;i++) { //First char first
#else
	for(int8_t i=2;i>=0;i--) { //Last char first
#endif
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
#ifdef SHREG_BACKWARDS
	for(int8_t i=0;i<3;i++) { //First char first
#else
	for(int8_t i=2;i>=0;i--) { //Last char first
#endif
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

#ifdef ATTINY84A
volatile uint16_t uartData = 0;

void uartTx(char valChar) {
	//9600 bps UART is used, with 8 data bits, no parity bits, and 1 stop bit.
	uartData = (uint16_t) valChar << 1; //LSB is for start bit
	uartData |= (1 << 9); //stop bit
	//configure timer
	OCR1A = 833; //sets compare/match register to 833, corresponding to 104µs (1 bit at 9600bd) at F_CPU = 8 MHz
	TIFR1 |= (1 << OCF1A); //clears any pending interrupt flags
	TIMSK1 = 1 << OCIE1A; //enables compare/match A interrupts
	//enable timer
	TCCR1B = (1 << WGM12) | (1 << CS10); //starts timer in CTC mode, with no prescaler
	//pull line low for start bit
	PORTA &= ~(1 << UARTX);
	while(uartData != 0);
}
#endif

void uartStr(char* valStr) {
#ifndef ATTINY84A
	return;
#else
	uint8_t leadingZeroes = 1;
	for(uint8_t i=0;i<11;i++) {
		if(valStr[i] == 0) {
			break;
		}
		if(leadingZeroes) {
			if(valStr[i] == '0') {
				continue;
			}
			leadingZeroes = 0;
		}
		uartTx(valStr[i]);
	}
	if(leadingZeroes) { //there have only been zeroes in the value, but none transmitted so far
		uartTx('0');
	}
	uartTx('\n');
#endif
}

void printTimeMicros(unsigned long timeMicros, uint8_t uartPrint) {
	char microsString[11]; //10 digits to accommodate max value of unsigned long (32 bit) in µs
	snprintf(microsString, 11, "%.10lu", timeMicros);
	if(uartPrint) {
		uartStr(microsString);
	}
	int8_t i;
	uint8_t mostSignificantDigit = 6; //index of microsString
	for(i=0;i<6;i++) {
		//find most significant digit
		if(microsString[i] != '0') {
			mostSignificantDigit = i;
			break;
		}
	}
	//rounding at 3rd most significant digit
	if(microsString[mostSignificantDigit + 3] > '4') { //4th most significant digit is greater than 4, rounding needed
		for(i=2;i>=0;i--) {
			//add 1 and carry, if needed
			if(microsString[mostSignificantDigit + i] == '9') {
				microsString[mostSignificantDigit + i] = '0';
				if(i==0 && mostSignificantDigit > 0) {
					mostSignificantDigit--;
					microsString[mostSignificantDigit] = '1';
				}
			} else {
				microsString[mostSignificantDigit + i] += 1;
				//no (further) carrying
				break;
			}
		}
	}

	for(i=0;i<10;i++) { //replace each ASCII digit with its segment representation
		microsString[i] -= '0';
		microsString[i] = digits[(uint8_t) microsString[i]];
	}
	if(mostSignificantDigit == 0) {mostSignificantDigit = 1;} //unable to display 4-digit seconds
	//add decimal and unit (seconds') points
	if(mostSignificantDigit < 4) { //value displayed in seconds
		microsString[3] |= (1<<7); //decimal point for seconds
		microsString[mostSignificantDigit+2] |= (1 << 7); //last digit gets an appended point to show seconds
	} else if(mostSignificantDigit > 4) { //value displayed in milliseconds with decimal point
		microsString[6] |= (1<<7); //decimal point for milliseconds
	}
#ifdef SHREG_BACKWARDS
	for(i=0;i<3;i++) { //First digit is transmitted first
#else
	for(i=2;i>=0;i--) { //Last digit is transmitted first
#endif
		shiftOut(microsString[mostSignificantDigit + i]);
	}
	display();
}

unsigned long calcTime() {
	unsigned long time;
#ifdef ATMEGA328P //F_CPU = 16 MHz, prescaler value = 64
	time = (unsigned long)timerOverflow << 10; //every timer overflow corresponds to 1024 µs
	time |= (unsigned long) TCNT0 << 2; //every remaining clock tick corresponds to 4 µs
#elif defined (ATTINY45) || defined (ATTINY84A) //F_CPU = 8 MHz, prescaler value = 8
	time = (unsigned long)timerOverflow << 8; //every timer overflow corresponds to 256 µs
	time |= (unsigned long) TCNT0; //every remaining clock tick corresponds to 1 µs
#endif
	return time;
}

uint8_t takeMeasurement(unsigned long *duration) {//takes a measurement and displays it. Interrupts measurement if button is pressed. Returns 1 if measurement completed, otherwise 0. If address != NULL is passed as argument, measured time is stored there.
	enableExtInt();
	unsigned long time;
	while(1) {
		if(measurementInProgress) {
			time = calcTime();
			printTimeMicros(time, UART_NO_PRINT);
		} else if(timerDone) {
			time = calcTime();
			printTimeMicros(time, UART_PRINT);
			timerDone = 0;
			if(duration != NULL) {
				*duration = time;
			}
			return 1;
		}
		if(!(IOPIN & (1 << PUSHBUTTON))) { //active LOW
			stopTimer();
			return 0;
		}
	}
}

int main(void) {
#if defined ATTINY84A
	IODDR |= _BV(SER) | _BV(RCLK) | _BV(SRCLK) | _BV(UARTX); //set these pins as outputs
	IOPORT |= _BV(UARTX); //pull UARTX pin high (UART line standby)
#else
	IODDR |= _BV(SER) | _BV(RCLK) | _BV(SRCLK); //set these pins as outputs
#endif
	IOPORT |= _BV(PUSHBUTTON); //enable pull-up

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
		printTimeMicros(0, UART_NO_PRINT);
		while(1) { //Normal (single-shot) operation
			if(!takeMeasurement(NULL)) { //button pressed
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
			//evaluation
			avgSum /= n;
			for(int i=0;i<3;i++) {
				threeCharPrint(titleList[i]);
				_delay_ms(2000);
				printTimeMicros(*valueList[i], UART_NO_PRINT);
				while((IOPIN & (1 << PUSHBUTTON))); //active LOW
			}
		}
		threeCharPrint(dashes);
		_delay_ms(1000);
	}

	return 0;
}

#if defined (ATMEGA328P) || defined (ATTINY45)
ISR(TIMER0_OVF_vect) {
#elif defined ATTINY84A
ISR(TIM0_OVF_vect) {
#endif
	timerOverflow++;
}

#if defined (ATMEGA328P) || defined (ATTINY45)
ISR(INT0_vect) {
#elif defined ATTINY84A
ISR(EXT_INT0_vect) {
#endif
	startOrStopTimer();
}

#if defined ATTINY84A
ISR(TIM1_COMPA_vect) {
	uartData = uartData >> 1;
	if(uartData == 0) { //finished transmitting, stop timer
		//last bit was stop bit, so no need to pull line up again for standby
		TCCR1B = (1 << WGM12); //stop timer
		TCNT1 = (uint16_t) 0; //reset counter
		return;
	}

	if(uartData & 0x01) {
		PORTA |= _BV(UARTX);
	} else {
		PORTA &= ~(1 << UARTX);
	}
}
#endif
