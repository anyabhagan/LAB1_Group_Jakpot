#ifndef SERIAL_PORT_H
#define SERIAL_PORT_H

/*
 * Thin C wrapper around the Arduino Serial objects.
 * This is the ONLY place the Arduino library is used, so that main.c
 * stays plain C and uses only sam.h and FreeRTOS for everything else.
 *
 *   Serial  - USB serial port: keys typed in the Serial Monitor arrive here
 *   Serial1 - hardware UART: TX on board pin 14, used to echo each received
 *             key so it can be viewed on an oscilloscope
 */

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void serial_init(void);           /* start the USB serial port and the TX-pin UART */
bool serial_rx_available(void);   /* true if a character has arrived (non-blocking) */
char serial_getc(void);           /* read one character; check serial_rx_available() first */
void serial_puts(const char *s);  /* send a string to the USB Serial Monitor */
void serial_tx_pin_putc(char c);  /* send one character out of the TX pin (pin 14) */

#ifdef __cplusplus
}
#endif

#endif