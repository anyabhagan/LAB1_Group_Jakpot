/*
 * ECNG 3006 - Microprocessors 2
 * Lab 1, Question 7: Debouncing serial RX finite state machine
 *
 * Name: Kaeden <SURNAME>
 * ID:   <STUDENT ID>
 *
 * Platform: Arduino MKR ZERO (ATSAMD21G18A), PlatformIO in VS Code.
 * <sam.h> (register-level access) and <FreeRTOS_SAMD21.h> are used for
 * everything except serial communication. The Arduino Serial library is
 * used ONLY for serial I/O, and only through the wrapper in serial_port.h.
 *
 * ---------------------------------------------------------------------------
 * FSM IMPLEMENTATION STRATEGY: if-else (Williams, Ch. 6)
 * ---------------------------------------------------------------------------
 * States : STATE_OFF (LED off), STATE_ON (LED on).
 *          This is a Moore machine: the LED output depends only on the state.
 *
 * Events : Following Williams' advice, incoming serial data is pre-processed
 *          into ONE FSM event per tick by get_event():
 *            EVENT_NONE            - no character this tick
 *            EVENT_SAME_CHAR_FAST  - same char, interval < 500 ms
 *            EVENT_SAME_CHAR_SLOW  - same char, interval > 500 ms
 *            EVENT_DIFFERENT_CHAR  - !same_char
 *
 * Transitions (from the state diagram):
 *            EVENT_DIFFERENT_CHAR or EVENT_SAME_CHAR_SLOW -> toggle ON/OFF
 *            EVENT_SAME_CHAR_FAST or EVENT_NONE           -> stay (self-loop)
 *
 *          The current state is tested with an if-else chain. Inside each
 *          branch, a nested if checks the event; each arc on the state
 *          diagram maps to one if condition, so the code can be read
 *          directly against the diagram. A final else catches any invalid
 *          state value and forces the FSM back to a safe state (OFF).
 *
 * Timing : The FSM runs ONLY on a 10 ms system tick, produced by a FreeRTOS
 *          task using vTaskDelayUntil(). 10 ms is a factor of 500 ms, so the
 *          debounce window is exactly 50 ticks. A tick counter is reset when
 *          it reaches 500 ms (marking the window as expired) or when a
 *          character is received (starting a new window).
 *
 * Serial : The MKR ZERO's USB serial port (Arduino Serial, 115200 baud).
 *          serial_rx_available() wraps Serial.available() and reports
 *          whether a character has arrived without blocking, so the tick is
 *          never held up waiting for input.
 *          Every received character is also echoed out of the hardware
 *          UART TX pin (board pin 14, 115200 8N1) so the serial frames can
 *          be observed on an oscilloscope.
 * ---------------------------------------------------------------------------
 */

#include <sam.h>
#include <FreeRTOS_SAMD21.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "serial_port.h"

/* ---------------------------- Configuration ------------------------------ */

#define TICK_PERIOD_MS        10U    /* FSM system tick period */
#define DEBOUNCE_MS           500U   /* repeat-character window */
#define DEBOUNCE_TICKS        (DEBOUNCE_MS / TICK_PERIOD_MS)   /* = 50 ticks */

#define LED_PORT_GROUP        1U          /* Port B */
#define LED_PIN_MASK          PORT_PB08   /* onboard LED on PB08 (active high) */

#define FSM_TASK_STACK_WORDS  512U

/* Debug aid: set to 1 to send 'U' (0x55 = 01010101) out of pin 14 on every
 * tick, independent of the keyboard. Used to check the oscilloscope setup.
 * Set back to 0 for normal operation. */
#define SCOPE_TEST_MODE       0
#define FSM_TASK_PRIORITY     (tskIDLE_PRIORITY + 1U)

/* ------------------------------- Types ----------------------------------- */

typedef enum {
	STATE_OFF = 0,
	STATE_ON
} fsm_state_t;

typedef enum {
	EVENT_NONE = 0,
	EVENT_SAME_CHAR_FAST,
	EVENT_SAME_CHAR_SLOW,
	EVENT_DIFFERENT_CHAR
} fsm_event_t;

/* --------------------------- Module variables ---------------------------- */

static uint32_t tick_counter = 0U;      /* ticks since the last character */
static bool window_expired = true;      /* true once 500 ms pass with no character */
static char last_char = '\0';           /* previous character received */
static bool have_last_char = false;     /* false until the first character arrives */

/* ---------------------------- LED (PORT) --------------------------------- */

static void led_init(void)
{
	PORT->Group[LED_PORT_GROUP].DIRSET.reg = LED_PIN_MASK;   /* output */
	PORT->Group[LED_PORT_GROUP].OUTCLR.reg = LED_PIN_MASK;   /* start off */
}

static void led_on(void)
{
	PORT->Group[LED_PORT_GROUP].OUTSET.reg = LED_PIN_MASK;
}

static void led_off(void)
{
	PORT->Group[LED_PORT_GROUP].OUTCLR.reg = LED_PIN_MASK;
}

/* ------------------------ Event pre-processing --------------------------- */

/*
 * Called once per tick. Reads at most one character and turns it, together
 * with the 500 ms tick counter, into a single FSM event.
 * Carriage return and line feed are treated as invalid input and ignored.
 */
static fsm_event_t get_event(void)
{
	fsm_event_t event = EVENT_NONE;

	if (serial_rx_available()) {
		char c = serial_getc();

		serial_tx_pin_putc(c);   /* echo every key to pin 14 for the scope */

		if ((c != '\r') && (c != '\n')) {
			if (!have_last_char || (c != last_char)) {
				event = EVENT_DIFFERENT_CHAR;
			} else if (window_expired) {
				event = EVENT_SAME_CHAR_SLOW;
			} else {
				event = EVENT_SAME_CHAR_FAST;
			}

			last_char = c;
			have_last_char = true;

			/* a character was received: restart the 500 ms window */
			tick_counter = 0U;
			window_expired = false;
			return event;
		}
	}

	/* no valid character this tick: advance the 500 ms counter */
	if (!window_expired) {
		tick_counter++;
		if (tick_counter >= DEBOUNCE_TICKS) {
			tick_counter = 0U;      /* reset once it hits 500 ms */
			window_expired = true;
		}
	}

	return event;
}

/* ----------------------------- FSM logic --------------------------------- */

/* Returns the next state for the given state and event (if-else FSM). */
static fsm_state_t fsm_next_state(fsm_state_t state, fsm_event_t event)
{
	fsm_state_t next = state;   /* default: self-loop, stay in this state */

	if (state == STATE_OFF) {
		if (event == EVENT_DIFFERENT_CHAR) {
			next = STATE_ON;    /* arc: !same_char */
		} else if (event == EVENT_SAME_CHAR_SLOW) {
			next = STATE_ON;    /* arc: same_char && interval > 500 ms */
		} else {
			next = STATE_OFF;   /* self-loop: interval < 500 ms or no input */
		}
	} else if (state == STATE_ON) {
		if (event == EVENT_DIFFERENT_CHAR) {
			next = STATE_OFF;   /* arc: !same_char */
		} else if (event == EVENT_SAME_CHAR_SLOW) {
			next = STATE_OFF;   /* arc: same_char && interval > 500 ms */
		} else {
			next = STATE_ON;    /* self-loop: interval < 500 ms or no input */
		}
	} else {
		next = STATE_OFF;           /* invalid state: recover safely */
	}

	return next;
}

/* Moore outputs: the LED depends only on the current state. */
static void fsm_apply_outputs(fsm_state_t state)
{
	if (state == STATE_ON) {
		led_on();
	} else {
		led_off();          /* STATE_OFF, or any invalid state */
	}
}

/* ---------------------------- FreeRTOS task ------------------------------ */

/* Runs the FSM exactly once per system tick. */
static void fsm_task(void *params)
{
	fsm_state_t state = STATE_OFF;
	TickType_t last_wake = xTaskGetTickCount();

	(void)params;
	fsm_apply_outputs(state);

	for (;;) {
		vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(TICK_PERIOD_MS));

#if SCOPE_TEST_MODE
		serial_tx_pin_putc('U');   /* test pattern for the scope */
#endif

		fsm_event_t event = get_event();
		fsm_state_t next = fsm_next_state(state, event);

		if (next != state) {
			state = next;
			fsm_apply_outputs(state);
			serial_puts((state == STATE_ON) ? "ON\r\n" : "OFF\r\n");
		}
	}
}

/* ------------------------------ Entry point ------------------------------ */

void setup(void)
{
	serial_init();
	led_init();

	xTaskCreate(fsm_task, "fsm", FSM_TASK_STACK_WORDS, NULL,
		    FSM_TASK_PRIORITY, NULL);
	vTaskStartScheduler();   /* never returns */
}

void loop(void)
{
	/* not reached once the scheduler is running */
}