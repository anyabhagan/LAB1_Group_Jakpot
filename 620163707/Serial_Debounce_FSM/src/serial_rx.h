/*
 * serial_rx.h
 *
 * C interface to the Arduino SAMD core serial library (SerialUSB).
 * The core's Serial object is C++ only, so serial_rx.cpp wraps the
 * three calls this program needs in C-callable functions.
 */

#ifndef SERIAL_RX_H
#define SERIAL_RX_H

#ifdef __cplusplus
extern "C" {                          /* C linkage when included from C++   */
#endif

void serial_rx_init(void);            /* open the USB serial port            */
int  serial_rx_getc(void);            /* non-blocking: char, or -1 if none;
                                         also echoed on TX pin 14 (Serial1) */
void serial_puts(const char *text);   /* print a line to the serial monitor  */

#ifdef __cplusplus
}
#endif

#endif /* SERIAL_RX_H */
