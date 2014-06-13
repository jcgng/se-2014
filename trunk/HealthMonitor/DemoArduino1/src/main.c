/**
 * \file
 *
 * \brief Empty user application template
 *
 */

/**
 * \mainpage User Application template doxygen documentation
 *
 * \par Empty user application template
 *
 * Bare minimum empty user application template
 *
 * \par Content
 *
 * -# Include the ASF header files (through asf.h)
 * -# Minimal main function that starts with a call to board_init()
 * -# "Insert application code here" comment
 *
 */

/*
 * Include header files for all drivers that have been imported from
 * Atmel Software Framework (ASF).
 */
#include <asf.h>
#include <avr/io.h>

int main(void) {
	long i;
	DDRB = 1<< DDB5; // PB5/D13 is an output
	while(1) {
		PORTB = 1<< PORTB5; // LED is on – alternativa: PORTB = 0x20;
		for(i = 0; i < 100000; i++); // delay
		PORTB = 0<< PORTB5; // LED is off – alternativa: PORTB = 0x00;
		for(i = 0; i < 100000; i++); // delay
	}
}

