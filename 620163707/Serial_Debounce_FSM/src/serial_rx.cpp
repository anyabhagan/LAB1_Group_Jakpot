/*
 * serial_rx.cpp
 *
 * Thin C wrapper around the Arduino SAMD core serial library.
 * serial_rx_getc() behaves like getch() in nodelay mode (Williams,
 * toaster4.c): it never blocks and returns -1 when nothing has arrived.
 */

#include <Arduino.h>
#include "serial_rx.h"

extern "C" void __libc_init_array(void);

/*
 * main.c replaces the Arduino main(), so this does the start-up the USB
 * serial library needs that Arduino's main() would normally do.
 */
void serial_rx_init(void)
{
    init();                 /* core clocks and timer used by the USB stack */
    __libc_init_array();    /* construct the C++ Serial/USB objects        */
    USBDevice.init();
    USBDevice.attach();

    /* No while(!Serial) wait: on the native-USB MKR Zero that would
       hang forever if no serial monitor is attached. */
    Serial.begin(115200);
    Serial1.begin(115200);  /* hardware UART: echo RX chars on TX pin 14  */
    delay(1000);           /* FreeRTOS_SAMD21 note: let the USB driver
                               start before the scheduler runs            */
}

int serial_rx_getc(void)
{
    if (Serial.available() > 0)
    {
        int c = Serial.read();
        Serial1.write(c);   /* put the received char on the TX pin */
        return c;
    }
    return -1;
}

void serial_puts(const char *text)
{
    Serial.println(text);
}
