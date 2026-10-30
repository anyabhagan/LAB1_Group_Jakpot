/*
 * ECNG 3006 - Microprocessors 2
 * Lab 1 - Data Acquisition System (ADC noise, SNR, ENOB, jitter, PMF)
 *
 * Name : Kaeden <SURNAME>
 * ID   : <ID NUMBER>
 *
 * Platform: Arduino MKR ZERO (SAMD21 @ 48 MHz), PlatformIO
 *
 * What it does:
 *  - TC4 interrupts every 1 ms (1000 Hz sampling rate).
 *  - In the ISR: record a timestamp from TC5 (48 MHz), start an ADC conversion,
 *    busy-wait for it to finish, and store the result.
 *  - 1000 samples go into DataBuf[], 1000 timestamps into TimeBuf[].
 *  - Main loop then calculates jitter, mean (signal), standard deviation
 *    (noise), SNR and ENOB using integer maths only, and prints a PMF.
 *  - This is repeated for hardware averaging of 1, 4, 16 and 64 samples.
 *
 * Wiring (potentiometer option, two resistors):
 *   MKR VCC (3.3 V) --- R1 10k ---+--- R2 20k --- MKR GND
 *                                  |
 *                               MKR A1 (PB02)     -> about 2.2 V
 *
 * Arduino library is used ONLY for Serial. Hardware is set up with <sam.h>.
 */

#include <Arduino.h>    /* used ONLY for Serial */
#include <sam.h>

#define N_SAMPLES      1000     /* samples per run                      */
#define TIMER_HZ       48000000 /* TC5 timestamp clock: 48 MHz, no prescaler */
#define VREF_MV        3300     /* ADC full scale in mV                 */
#define ADC_COUNTS        4096     /* 12-bit ADC                           */

/* Hardware averaging settings to test (keep <= 64 so the ISR stays well
 * under 1 ms: 64 conversions take roughly 0.35 ms) */
const uint16_t avg_list[] = {1, 4, 16, 64};
#define N_AVG_SETTINGS  4

/* Buffers filled by the ISR */
volatile uint16_t DataBuf[N_SAMPLES];
volatile uint16_t TimeBuf[N_SAMPLES];
volatile uint16_t sample_index = 0;
volatile bool     buffer_full = false;

/* ------------------------------------------------------------------ */
/* Small integer maths helpers (no floating point allowed)            */
/* ------------------------------------------------------------------ */

/* Integer square root (bit-by-bit method) */
uint32_t isqrt64(uint64_t n)
{
    uint64_t result = 0;
    uint64_t bit = (uint64_t)1 << 62;

    while (bit > n) {
        bit >>= 2;
    }
    while (bit != 0) {
        if (n >= result + bit) {
            n -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }
    return (uint32_t)result;
}

/* Integer log2 (floor): how many times can x be halved before it is 1 */
uint8_t ilog2(uint32_t x)
{
    uint8_t n = 0;

    while (x > 1) {
        x >>= 1;
        n++;
    }
    return n;
}

/* Prints a value stored as (value x 100) with two decimal places */
void print_x100(uint32_t value_x100)
{
    Serial.print(value_x100 / 100);
    Serial.print(".");
    if ((value_x100 % 100) < 10) {
        Serial.print("0");
    }
    Serial.print(value_x100 % 100);
}

/* ------------------------------------------------------------------ */
/* TC4: 1 ms periodic interrupt (same set-up as the cyclic executive)  */
/* ------------------------------------------------------------------ */
void sample_timer_init(void)
{
    PM->APBCMASK.reg |= PM_APBCMASK_TC4 | PM_APBCMASK_TC5;

    /* 48 MHz GCLK0 -> TC4 and TC5 (they share one clock channel) */
    GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID_TC4_TC5 |
                        GCLK_CLKCTRL_GEN_GCLK0 |
                        GCLK_CLKCTRL_CLKEN;
    while (GCLK->STATUS.bit.SYNCBUSY);

    TC4->COUNT16.CTRLA.reg = TC_CTRLA_SWRST;
    while (TC4->COUNT16.CTRLA.bit.SWRST);

    /* 48 MHz / 64 = 750 kHz, count 0..749 -> 1 ms */
    TC4->COUNT16.CTRLA.reg = TC_CTRLA_MODE_COUNT16 |
                             TC_CTRLA_WAVEGEN_MFRQ |
                             TC_CTRLA_PRESCALER_DIV64;
    while (TC4->COUNT16.STATUS.bit.SYNCBUSY);

    TC4->COUNT16.CC[0].reg = 749;
    while (TC4->COUNT16.STATUS.bit.SYNCBUSY);

    TC4->COUNT16.INTENSET.reg = TC_INTENSET_MC0;
    NVIC_SetPriority(TC4_IRQn, 0);      /* highest priority = less jitter */
    NVIC_EnableIRQ(TC4_IRQn);
    /* TC4 is started later, in capture_samples() */
}

/* ------------------------------------------------------------------ */
/* TC5: free-running 16-bit counter at 48 MHz for timestamps           */
/* One count = 20.8 ns. Wraps every 1.365 ms, which is fine because we  */
/* only subtract neighbouring timestamps that are ~1 ms apart.          */
/* ------------------------------------------------------------------ */
void timestamp_timer_init(void)
{
    TC5->COUNT16.CTRLA.reg = TC_CTRLA_SWRST;
    while (TC5->COUNT16.CTRLA.bit.SWRST);

    TC5->COUNT16.CTRLA.reg = TC_CTRLA_MODE_COUNT16 |
                             TC_CTRLA_PRESCALER_DIV1;
    while (TC5->COUNT16.STATUS.bit.SYNCBUSY);

    TC5->COUNT16.CTRLA.reg |= TC_CTRLA_ENABLE;
    while (TC5->COUNT16.STATUS.bit.SYNCBUSY);
}

/* Reads the TC5 count (the COUNT register must be synchronised first) */
uint16_t read_timestamp(void)
{
    TC5->COUNT16.READREQ.reg = TC_READREQ_RREQ |
                               TC_READREQ_ADDR(TC_COUNT16_COUNT_OFFSET);
    while (TC5->COUNT16.STATUS.bit.SYNCBUSY);
    return TC5->COUNT16.COUNT.reg;
}

/* ------------------------------------------------------------------ */
/* ADC on A1 (PB02 = AIN10)                                            */
/* Reference: VDDANA/2 with gain 1/2 -> 0 to 3.3 V input range          */
/* (factory calibration is already loaded by the startup code)          */
/* ------------------------------------------------------------------ */
void adc_init(void)
{
    PM->APBCMASK.reg |= PM_APBCMASK_ADC;

    /* 48 MHz GCLK0 -> ADC (divided by 32 below = 1.5 MHz ADC clock) */
    GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID_ADC |
                        GCLK_CLKCTRL_GEN_GCLK0 |
                        GCLK_CLKCTRL_CLKEN;
    while (GCLK->STATUS.bit.SYNCBUSY);

    /* PB02 -> peripheral function B (analog) */
    PORT->Group[1].PINCFG[2].bit.PMUXEN = 1;
    PORT->Group[1].PMUX[2 >> 1].bit.PMUXE = 1;   /* 1 = function B */

    ADC->CTRLA.bit.ENABLE = 0;
    while (ADC->STATUS.bit.SYNCBUSY);

    ADC->REFCTRL.reg = ADC_REFCTRL_REFSEL_INTVCC1;        /* VDDANA / 2 */

    ADC->INPUTCTRL.reg = ADC_INPUTCTRL_MUXPOS_PIN10 |      /* A1         */
                         ADC_INPUTCTRL_MUXNEG_GND |         /* single-ended */
                         ADC_INPUTCTRL_GAIN_DIV2;
    while (ADC->STATUS.bit.SYNCBUSY);

    ADC->SAMPCTRL.reg = ADC_SAMPCTRL_SAMPLEN(3);   /* longer sampling time */
}

/* Selects hardware averaging: samples = 1, 2, 4, ... 64
 * The result is always scaled back to 12 bits so runs can be compared. */
void adc_set_averaging(uint16_t samples)
{
    uint8_t samplenum = ilog2(samples);              /* 1->0, 4->2, 16->4 */
    uint8_t adjres = (samplenum > 4) ? 4 : samplenum;

    ADC->CTRLA.bit.ENABLE = 0;
    while (ADC->STATUS.bit.SYNCBUSY);

    if (samples == 1) {
        ADC->CTRLB.reg = ADC_CTRLB_PRESCALER_DIV32 | ADC_CTRLB_RESSEL_12BIT;
    } else {
        /* averaging requires the 16-bit result mode */
        ADC->CTRLB.reg = ADC_CTRLB_PRESCALER_DIV32 | ADC_CTRLB_RESSEL_16BIT;
    }
    while (ADC->STATUS.bit.SYNCBUSY);

    ADC->AVGCTRL.reg = ADC_AVGCTRL_SAMPLENUM(samplenum) |
                       ADC_AVGCTRL_ADJRES(adjres);

    ADC->CTRLA.bit.ENABLE = 1;
    while (ADC->STATUS.bit.SYNCBUSY);
}

/* One conversion with a busy wait (as the lab asks) */
uint16_t adc_read(void)
{
    ADC->INTFLAG.reg = ADC_INTFLAG_RESRDY;     /* clear old result flag */
    ADC->SWTRIG.bit.START = 1;
    while (ADC->INTFLAG.bit.RESRDY == 0);       /* busy wait */
    while (ADC->STATUS.bit.SYNCBUSY);
    return ADC->RESULT.reg;
}

/* ------------------------------------------------------------------ */
/* 1 kHz ISR: timestamp, sample, store                                 */
/* ------------------------------------------------------------------ */
extern "C" void TC4_Handler(void)
{
    TC4->COUNT16.INTFLAG.reg = TC_INTFLAG_MC0;     /* clear the flag */

    if (sample_index < N_SAMPLES) {
        TimeBuf[sample_index] = read_timestamp();
        DataBuf[sample_index] = adc_read();
        sample_index++;
    } else {
        buffer_full = true;
        TC4->COUNT16.CTRLA.bit.ENABLE = 0;         /* stop sampling */
    }
}

/* Starts the 1 kHz timer and waits until 1000 samples are collected */
void capture_samples(void)
{
    sample_index = 0;
    buffer_full = false;

    TC4->COUNT16.COUNT.reg = 0;
    while (TC4->COUNT16.STATUS.bit.SYNCBUSY);
    TC4->COUNT16.CTRLA.bit.ENABLE = 1;
    while (TC4->COUNT16.STATUS.bit.SYNCBUSY);

    while (!buffer_full) {
        /* wait - do NOT print here, Serial activity adds jitter */
    }
}

/* ------------------------------------------------------------------ */
/* Calculations and printing                                           */
/* ------------------------------------------------------------------ */

/* Jitter = (largest gap) - (smallest gap) between neighbouring samples */
void report_jitter(void)
{
    uint16_t maxT = 0;
    uint16_t minT = 0xFFFF;

    for (int i = 1; i < N_SAMPLES; i++) {
        uint16_t gap = (uint16_t)(TimeBuf[i] - TimeBuf[i - 1]); /* wrap-safe */
        if (gap > maxT) maxT = gap;
        if (gap < minT) minT = gap;
    }

    uint16_t jitter = maxT - minT;

    /* counts -> ns: 1 count = 1e9 / 48e6 = 20.83 ns */
    Serial.print("  Gaps (TC5 counts, ideal 48000): min = ");
    Serial.print(minT);
    Serial.print(", max = ");
    Serial.println(maxT);
    Serial.print("  Jitter = ");
    Serial.print(jitter);
    Serial.print(" counts = ");
    Serial.print((uint32_t)jitter * 1000000UL / (TIMER_HZ / 1000));
    Serial.println(" ns");
}

/* Mean (signal), standard deviation (noise), SNR, ENOB - integers only */
void report_noise(void)
{
    uint32_t sum = 0;
    uint64_t sumsq = 0;

    for (int i = 0; i < N_SAMPLES; i++) {
        sum += DataBuf[i];
        sumsq += (uint32_t)DataBuf[i] * DataBuf[i];
    }

    /* mean x 100 */
    uint32_t mean_x100 = sum * 100UL / N_SAMPLES;

    /* variance x 10000 = (N*sumsq - sum^2) * 10000 / N^2,
     * so sqrt gives the standard deviation x 100 */
    uint64_t var_x10000 = ((uint64_t)N_SAMPLES * sumsq - (uint64_t)sum * sum)
                          * 10000ULL / ((uint64_t)N_SAMPLES * N_SAMPLES);
    uint32_t sd_x100 = isqrt64(var_x10000);

    Serial.print("  Signal (mean) = ");
    print_x100(mean_x100);
    Serial.print(" LSB = ");
    print_x100(mean_x100 * VREF_MV / ADC_COUNTS);
    Serial.println(" mV");

    Serial.print("  Noise (SD)    = ");
    print_x100(sd_x100);
    Serial.print(" LSB = ");
    print_x100(sd_x100 * VREF_MV / ADC_COUNTS);
    Serial.println(" mV");

    if (sd_x100 == 0) {
        Serial.println("  SD is 0 (below 0.01 LSB) - SNR too large to calculate");
        return;
    }

    uint32_t snr_x100 = mean_x100 * 100UL / sd_x100;

    Serial.print("  SNR = ");
    print_x100(snr_x100);
    Serial.print("   ENOB = log2(SNR) = ");
    Serial.print(ilog2(snr_x100 / 100));
    Serial.println(" bits (whole bits)");
}

/* PMF: how many samples landed on each ADC value (CSV for Excel) */
void report_pmf(void)
{
    uint16_t lo = 0xFFFF;
    uint16_t hi = 0;

    for (int i = 0; i < N_SAMPLES; i++) {
        if (DataBuf[i] < lo) lo = DataBuf[i];
        if (DataBuf[i] > hi) hi = DataBuf[i];
    }

    Serial.println("  PMF (ADC value,count):");
    for (uint16_t value = lo; value <= hi; value++) {
        uint16_t count = 0;
        for (int i = 0; i < N_SAMPLES; i++) {
            if (DataBuf[i] == value) count++;
        }
        Serial.print(value);
        Serial.print(",");
        Serial.println(count);
    }
}

/* ------------------------------------------------------------------ */
void setup()
{
    Serial.begin(115200);
    while (!Serial);            /* wait for the serial monitor */

    adc_init();
    sample_timer_init();
    timestamp_timer_init();

    Serial.println("ECNG 3006 Lab 1 - Data Acquisition");
    Serial.println("Kaeden <SURNAME> <ID NUMBER>");
    Serial.println();
}

void loop()
{
    for (int k = 0; k < N_AVG_SETTINGS; k++) {
        adc_set_averaging(avg_list[k]);
        (void)adc_read();       /* throw away first result after a change */

        capture_samples();

        Serial.print("===== Hardware averaging: ");
        Serial.print(avg_list[k]);
        Serial.println(" sample(s) =====");
        report_jitter();
        report_noise();
        report_pmf();
        Serial.println();
    }

    Serial.println("Done. Press Enter in the serial monitor to run again.");
    while (Serial.available() == 0);
    while (Serial.available() > 0) {
        (void)Serial.read();    /* empty the input */
    }
    Serial.println();
}