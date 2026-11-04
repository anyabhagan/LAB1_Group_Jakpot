#include <Arduino.h>

#include "serial_port.h"

#define USB_SERIAL_BAUD  115200UL   /* ignored by USB CDC, but required by begin() */
#define TX_PIN_BAUD      115200UL   /* hardware UART on pin 14: 1 bit = ~8.68 us */

void serial_init(void)
{
	Serial.begin(USB_SERIAL_BAUD);    /* USB: keyboard input from the PC */
	Serial1.begin(TX_PIN_BAUD);       /* UART: TX on pin 14 for the oscilloscope */
}

bool serial_rx_available(void)
{
	return Serial.available() > 0;
}

char serial_getc(void)
{
	int c = Serial.read();          /* returns -1 if nothing is waiting */
	return (c < 0) ? '\0' : (char)c;
}

void serial_puts(const char *s)
{
	Serial.print(s);
}

void serial_tx_pin_putc(char c)
{
	Serial1.write((uint8_t)c);
}