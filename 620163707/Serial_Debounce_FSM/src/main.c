/* main.c, ECNG3006 Lab 1 Question 7 - debouncing serial RX FSM

     Implemented using FST with table dispatcher
     (Williams, Ch. 6.6, following toaster4.c).

     Name: Joseph berry
     ID:   620163707

     Platform: Arduino MKR Zero (SAMD21), PlatformIO,
               FreeRTOS (briscoetech/FreeRTOS_SAMD21), CMSIS <sam.h>.
               The Arduino core is used ONLY inside serial_rx.cpp as the
               USB serial library; nothing in this file calls Arduino.

   BEHAVIOUR
     Each character received is judged on its own. A different character,
     or the same character after 500 ms or more, toggles the state
     (OFF <-> ON). The same character within 500 ms is ignored.
     State ON = LED on, state OFF = LED off. CR/LF are discarded so a
     serial monitor's line ending is not seen as a different character.

   STRATEGY (FST method, as in toaster4.c)
     - The state model is held in data, not code: state_table[event][state]
       holds {active, next_state, action}. The running position of the
       model is held in the state index variable 'state'.
     - States and events are numbered from 0 to index the table.
     - The slow laundry turns the received character and the 500 ms
       interval counter into event flags in 'events'.
     - getmask() builds the current state's event mask from the FST;
       geteventn() uses it to pick the highest priority event (top row
       first), cancels its flag and returns its number.
     - The periodic dispatcher calls the cell's ACTION, loads the next
       state, and clears the event flags if the state changed.
     - geteventn() returns 0 for "no event" (as in the book), so event 0
       is a NOEVENT row that is inactive in every state; real events
       start at 1.
     - The dispatcher is entered only on the 10 ms system tick. Instead
       of the book's times() wait loop, a FreeRTOS task blocks in
       vTaskDelayUntil() until each tick. 500 ms is 50 ticks, so the
       interval is a tick counter reset when it hits 50 or a chaeeeeer arrives.
*/

#include <sam.h>
#include <FreeRTOS.h>
#include <task.h>
#include "serial_rx.h"

#define STATESX 2         // number of states

#define EVENTSX 4         // number of distinct events

#define TICKMS 10         // system tick period, ms

#define DEBOUNCETICKS 50  // 50 ticks x 10 ms = 500 ms

#define FSMSTACK 256      // FSM task stack, words

#define LEDPORT 1         // on-board LED is PB08: port group B,

#define LEDMASK PORT_PB08 // pin 8

                          // definition of event flags

#define NOEVENT 0x01      //event 0 never dispatched, see above

#define SAMESLOW 0x02     //event 1 same char, interval >= 500 ms

#define NEWCHAR 0x04      //event 2 different char

#define SAMEFAST 0x08     //event 3 same char, interval < 500 ms

unsigned int events, state, state_events;

unsigned int tick_count, interval_expired;

int key, last_key = -1;

void null() { }

void led_on() {
   PORT->Group[LEDPORT].OUTSET.reg = LEDMASK;
   serial_puts("OFF -> ON  : LED on");
}

void led_off() {
   PORT->Group[LEDPORT].OUTCLR.reg = LEDMASK;
   serial_puts("ON -> OFF  : LED off");
}

void ignore() {
   serial_puts("same char within 500 ms : ignored");
}

typedef void (*TPT)();

struct state_table {
unsigned int active;
unsigned int next_state;
TPT action;
} state_table[EVENTSX][STATESX] = {

// Off           On              // Events

{ {0,0,null},    {0,1,null}    },//0 NoEvent
{ {1,1,led_on},  {1,0,led_off} },//1 SameSlow
{ {1,1,led_on},  {1,0,led_off} },//2 NewChar
{ {1,0,ignore},  {1,1,ignore}  },//3 SameFast
};

/* getmask, builds an event bit mask for a state from the FST
* enabling the dispatcher to eliminate irrelevant events
*/
unsigned int getmask(unsigned int staten) {

     unsigned int mask=0;
     int i;
     for(i = EVENTSX-1; i >= 0; i--) {

          mask <<=1;
          mask |= state_table[i][staten].active;
     }
return mask;
}

/* geteventn, using the current state's event mask to select

* only relevant events it then identifies the active event

* with the highest priority, returning its number

*/

unsigned int geteventn() {

unsigned int i, bitmask = 1;

state_events = events & getmask(state);

for(i=0; i < EVENTSX; i++) {             //mask out irrelevant
                                         //events
     if (state_events & bitmask) {

        events &= ~bitmask;              //cancel selected event
                                         //flag
        return i;

     }
     bitmask <<= 1;
}
return 0;
}

/* fsm_task, the FreeRTOS task holding the FSM main loop
*/
void fsm_task(void *params) {

unsigned int last_state, event, last_event=0;

TickType_t next_tick;

(void)params;
next_tick = xTaskGetTickCount();

for (;;) {

      vTaskDelayUntil(&next_tick, pdMS_TO_TICKS(TICKMS));
                                                // wait for 10 ms tick

                                                // slow cyclic scanner
                                                // section
      if (!interval_expired) {

            if (++tick_count >= DEBOUNCETICKS) {

                  interval_expired = 1;         // 500 ms reached,
                  tick_count = 0;               // reset counter
            }
      }

      if ((key = serial_rx_getc()) != -1) {

                                                // slow laundry
                                                // Set event flags from
                                                // data etc

            if (key != '\r' && key != '\n') {

                  if (key != last_key) events |= NEWCHAR;

                  else if (interval_expired) events |= SAMESLOW;

                  else events |= SAMEFAST;

                  last_key = key;               // char received,
                  tick_count = 0;               // reset counter
                  interval_expired = 0;
            }
      }

                                                // periodic dispatcher

      if ( (event = geteventn()) != 0 ) {

            last_event = event;

            last_state = state;

            (*(state_table[event][state].action))();     // transition
                                                         // ACTION

            state = state_table[event][state].next_state;// Get next
                                                         // state

            if(state != last_state) events = 0;          // Clear event
                                                         // flags
      }                                                  // if change
                                                         // of state
      (void)last_event;     // book reads it in PRINTdebug, not used here
}
}

/* Main application entry point */
int main(void) {

serial_rx_init();                                // USB serial first,
                                                 // it resets pins
PORT->Group[LEDPORT].DIRSET.reg = LEDMASK;       // LED pin as output
PORT->Group[LEDPORT].OUTCLR.reg = LEDMASK;       // start in OFF
state = 0;

xTaskCreate(fsm_task, "FSM", FSMSTACK, NULL, tskIDLE_PRIORITY + 1, NULL);
vTaskStartScheduler();

while (1) {
      // Will never get executed
}
return 0;
}

// Keep the underlying compilation framework happy: the FreeRTOS
// library's idle hook calls loop(), so both must exist
void setup(void) { }
void loop(void) { }
