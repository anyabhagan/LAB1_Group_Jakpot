
#include <Arduino.h>
#include <sam.h>
#include <stdint.h>

// Name: Anya Bhagan
// Student ID: 816041932
// Course: ECNG3006
// Assignment: Cyclic Executive Scheduler
// Platform: Arduino MKR Zero - ATSAMD21G18A
// Method: Table-based cyclic executive
// Timer: TC5 interrupt every 1 ms

// ------------------------------------------
// Scheduler settings
// ------------------------------------------

#define NUM_SLOTS 4
#define TASKS_PER_SLOT 2
#define SLOT_TIME_MS 1000UL

// ------------------------------------------
// Timekeeping structure
// ------------------------------------------

typedef struct
{
    uint32_t totalMilliseconds;
    uint32_t seconds;
    uint16_t milliseconds;
} SystemTime;

// Shared between interrupt and main program.
volatile SystemTime systemTime = {0, 0, 0};

// ------------------------------------------
// TC5 interrupt service routine
// ------------------------------------------

void TC5_Handler(void)
{
    if (TC5->COUNT16.INTFLAG.bit.MC0)
    {
        // Acknowledge the timer interrupt.
        TC5->COUNT16.INTFLAG.reg = TC_INTFLAG_MC0;

        // Update the software clock.
        systemTime.totalMilliseconds++;
        systemTime.milliseconds++;

        if (systemTime.milliseconds >= 1000U)
        {
            systemTime.milliseconds = 0;
            systemTime.seconds++;
        }
    }
}

// ------------------------------------------
// Configure TC5 for a 1 ms timer interrupt
// ------------------------------------------

void timer_init(void)
{
    // Enable the generic clock for TC4/TC5.
    GCLK->CLKCTRL.reg =
        GCLK_CLKCTRL_CLKEN |
        GCLK_CLKCTRL_GEN_GCLK0 |
        GCLK_CLKCTRL_ID_TC4_TC5;

    while (GCLK->STATUS.bit.SYNCBUSY)
    {
    }

    // Disable TC5 before configuration.
    TC5->COUNT16.CTRLA.reg = 0;

    while (TC5->COUNT16.STATUS.bit.SYNCBUSY)
    {
    }

    // Reset TC5.
    TC5->COUNT16.CTRLA.reg = TC_CTRLA_SWRST;

    while (TC5->COUNT16.STATUS.bit.SYNCBUSY ||
           TC5->COUNT16.CTRLA.bit.SWRST)
    {
    }

    // Configure 16-bit mode, match-frequency
    // operation, and a clock prescaler of 16.
    TC5->COUNT16.CTRLA.reg =
        TC_CTRLA_MODE_COUNT16 |
        TC_CTRLA_WAVEGEN_MFRQ |
        TC_CTRLA_PRESCALER_DIV16;

    // At a 48 MHz clock:
    // 48,000,000 / 16 = 3,000,000 Hz
    // 3,000 counts = 1 millisecond.
    TC5->COUNT16.CC[0].reg = 2999;

    while (TC5->COUNT16.STATUS.bit.SYNCBUSY)
    {
    }

    // Clear any pending timer flag.
    TC5->COUNT16.INTFLAG.reg = TC_INTFLAG_MC0;

    // Enable match interrupt.
    TC5->COUNT16.INTENSET.reg = TC_INTENSET_MC0;

    // Enable TC5 in the interrupt controller.
    NVIC_ClearPendingIRQ(TC5_IRQn);
    NVIC_SetPriority(TC5_IRQn, 2);
    NVIC_EnableIRQ(TC5_IRQn);

    // Start the hardware timer.
    TC5->COUNT16.CTRLA.reg |= TC_CTRLA_ENABLE;

    while (TC5->COUNT16.STATUS.bit.SYNCBUSY)
    {
    }
}

// ------------------------------------------
// Custom time function
// ------------------------------------------

uint32_t time_ms(void)
{
    // Aligned 32-bit reads are atomic
    // on the Cortex-M0+.
    return systemTime.totalMilliseconds;
}

// ------------------------------------------
// Custom sleep function
// ------------------------------------------

void sleep_ms(uint32_t duration)
{
    uint32_t startTime = time_ms();

    while ((uint32_t)(time_ms() - startTime) < duration)
    {
        // Sleep CPU until an interrupt occurs.
        __WFI();
    }
}

// ------------------------------------------
// Sleep until an absolute deadline
// ------------------------------------------

void sleep_until(uint32_t deadline)
{
    while ((int32_t)(time_ms() - deadline) < 0)
    {
        __WFI();
    }
}

// ------------------------------------------
// Serial output helper
// ------------------------------------------

void print_task(const char *taskName)
{
    if (Serial)
    {
        Serial.print("[");
        Serial.print(time_ms());
        Serial.print(" ms] ");
        Serial.println(taskName);
    }
}

// ------------------------------------------
// Demonstration tasks
// ------------------------------------------

void task_one(void)
{
    print_task("Task 1 running");
    sleep_ms(100);
}

void task_two(void)
{
    print_task("Task 2 running");
    sleep_ms(200);
}

void task_three(void)
{
    print_task("Task 3 running");
    sleep_ms(300);
}

void task_four(void)
{
    print_task("Task 4 running");
    sleep_ms(400);
}

void task_five(void)
{
    print_task("Task 5 running");
    sleep_ms(500);
}

// ------------------------------------------
// Task dispatch table
// ------------------------------------------

// Each row is one scheduling slot.
// Every slot lasts 1000 ms.

void (*const taskTable[NUM_SLOTS][TASKS_PER_SLOT])(void) =
{
    {task_one, task_two},
    {task_one, task_three},
    {task_one, task_four},
    {task_five, nullptr}
};

// ------------------------------------------
// Burn time until the next slot
// ------------------------------------------

void burn(uint32_t slotDeadline)
{
    uint32_t burnStart = time_ms();

    if ((int32_t)(slotDeadline - burnStart) > 0)
    {
        sleep_until(slotDeadline);

        if (Serial)
        {
            Serial.print("Burn time = ");
            Serial.print(time_ms() - burnStart);
            Serial.println(" ms");
        }
    }
    else if (Serial)
    {
        Serial.println("WARNING: Slot overrun!");
    }
}

// ------------------------------------------
// Arduino framework initialization
// ------------------------------------------

void setup(void)
{
    // Initialize the hardware timer first.
    timer_init();

    // Start USB serial communication.
    Serial.begin(115200);

    // Give the computer time to open
    // the Serial Monitor.
    uint32_t startTime = time_ms();

    while (!Serial &&
           (time_ms() - startTime < 5000UL))
    {
        __WFI();
    }

    Serial.println();
    Serial.println("ECNG3006 - Cyclic Executive");
    Serial.println("Arduino MKR Zero");
    Serial.println("TC5 hardware timer: 1 ms tick");
    Serial.println("Slot duration: 1000 ms");
    Serial.println();
}

// ------------------------------------------
// Cyclic executive main scheduler
// ------------------------------------------

void loop(void)
{
    static uint32_t nextSlotStart = 0;
    static uint8_t currentSlot = 0;
    static uint32_t cycleNumber = 0;

    if (nextSlotStart == 0)
    {
        nextSlotStart = time_ms();
    }

    if (currentSlot == 0 && Serial)
    {
        Serial.print("=== Cycle ");
        Serial.print(cycleNumber);
        Serial.println(" ===");
    }

    // Wait for the slot start.
    sleep_until(nextSlotStart);

    uint32_t slotDeadline =
        nextSlotStart + SLOT_TIME_MS;

    if (Serial)
    {
        Serial.print("Starting Slot ");
        Serial.println(currentSlot);
    }

    // Execute tasks listed in this slot.
    for (uint8_t i = 0; i < TASKS_PER_SLOT; i++)
    {
        if (taskTable[currentSlot][i] != nullptr)
        {
            taskTable[currentSlot][i]();
        }
    }

    // Wait for the slot to finish.
    burn(slotDeadline);

    // Advance the scheduled time.
    nextSlotStart += SLOT_TIME_MS;

    // Advance the slot number.
    currentSlot++;

    if (currentSlot >= NUM_SLOTS)
    {
        currentSlot = 0;
        cycleNumber++;
    }
}
