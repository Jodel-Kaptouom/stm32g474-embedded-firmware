# Module 06: Bare-Metal ADC Analog Conversion using ADC1

This module implements a **register-level ADC1 driver** on the STM32G474RE, without the STM32 HAL ADC driver.

The firmware measures the analog voltage on **PA0**, converts it to millivolts with integer arithmetic and prints the result over the USART2 link from Module 05.

As an integration exercise, the measurement also sets the TIM2 PWM duty cycle from Module 04, which turns a potentiometer and the LED on PA5 into a **light dimmer**.

---

## Table of Contents

* [Objectives](#objectives)
* [Hardware Configuration](#hardware-configuration)
* [Project Structure](#project-structure)
* [ADC1 Configuration](#adc1-configuration)
* [Conversion](#conversion)
* [Voltage Calculation](#voltage-calculation)
* [Application](#application)
* [Integration Exercise](#integration-exercise-potentiometer-controlled-led-dimmer)
* [Verification](#verification)
* [Key Registers](#key-registers)
* [Engineering Notes](#engineering-notes)
* [Module Outcome](#module-outcome)
* [Next Module](#next-module)

---

# Objectives

* Configure ADC1 using direct register access.
* Configure PA0 as an analog input.
* Select the ADC clock source.
* Follow the ADC power-up sequence: deep-power-down exit, voltage regulator, calibration, enable.
* Set the sampling time and the conversion sequence.
* Run a single software-triggered conversion by polling.
* Convert the raw 12-bit result to millivolts without floating point.
* Print the measurement over USART2 with `printf`.
* Scale the ADC result to the PWM range to control the LED brightness with a potentiometer.

---

# Hardware Configuration

| Item            | Value                                     |
| --------------- | ----------------------------------------- |
| Microcontroller | STM32G474RE (ARM Cortex-M4)               |
| Peripheral      | ADC1                                      |
| Input           | `PA0` = ADC1 channel 1 (`ADC1_IN1`)       |
| Pin mode        | Analog                                    |
| ADC clock       | Synchronous, HCLK / 1 = 16 MHz            |
| Resolution      | 12 bits (0 to 4095), right-aligned        |
| Reference       | 3.3 V                                     |
| Conversion mode | Single conversion, software trigger       |
| Sampling time   | 247.5 ADC clock cycles                    |
| Output          | USART2 over the ST-LINK Virtual COM Port  |

Signal path:

```text
Analog voltage (0 to 3.3 V)
        │
        ▼
PA0 (analog mode)
        │
        ▼
ADC1 channel 1 ── sample & hold ── 12-bit conversion
        │
        ▼
ADC1->DR (0 to 4095)
        │
        ▼
Millivolt calculation
        │
        ▼
printf() ── USART2 ── Serial terminal
```

---

# Project Structure

The ADC driver lives in its own source and header files, like the UART and PWM drivers:

```text
adc.c / adc.h     ADC1 driver
uart.c / uart.h   USART2 driver (Module 05)
pwm.c / pwm.h     TIM2 PWM driver (Module 04)
main.c            Application
```

Driver interface (`adc.h`):

```c
void     ADC1_Init(void);
uint16_t ADC1_read(void);
```

---

# ADC1 Configuration

### 1. Clocks

```c
RCC->AHB2ENR |= (1U << 13);   /* ADC12 */
RCC->AHB2ENR |= (1U << 0);    /* GPIOA */
```

### 2. PA0 in analog mode

```c
GPIOA->MODER |= (3U << 0);    /* MODER0 = 11b */
```

### 3. ADC clock source

ADC1 and ADC2 share a common clock configuration. `CKMODE = 01` selects the synchronous clock HCLK / 1:

```c
ADC12_COMMON->CCR &= ~(3U << 16);
ADC12_COMMON->CCR |=  (1U << 16);
```

### 4. Power-up sequence

After reset the ADC is in deep-power-down mode with its internal voltage regulator off. Both must be handled before anything else:

```c
ADC1->CR &= ~(1U << 29);                   /* DEEPPWD = 0  */
ADC1->CR |=  (1U << 28);                   /* ADVREGEN = 1 */

for (volatile int i = 0; i < 1000; i++);   /* regulator start-up, about 20 µs */
```

### 5. Calibration

Calibration runs while the ADC is still disabled. `ADCAL` is set by software and cleared by hardware when the calibration is complete:

```c
ADC1->CR &= ~(1U << 30);          /* ADCALDIF = 0: single-ended */
ADC1->CR |=  (1U << 31);          /* ADCAL = 1: start           */
while (ADC1->CR & (1U << 31));    /* wait until ADCAL = 0       */
```

### 6. Enable

```c
ADC1->ISR = (1U << 0);               /* clear ADRDY by writing 1 */
ADC1->CR |= (1U << 0);               /* ADEN = 1                 */
while (!(ADC1->ISR & (1U << 0)));    /* wait until ADRDY = 1     */
```

### 7. Sampling time

`SMP1 = 110b` gives 247.5 ADC clock cycles for channel 1:

```c
ADC1->SMPR1 &= ~(7U << 3);
ADC1->SMPR1 |=  (6U << 3);
```

```text
Sampling time   = 247.5 / 16 MHz           = 15.5 µs
Conversion time = (247.5 + 12.5) / 16 MHz  = 16.25 µs
```

### 8. Conversion sequence

One conversion, on channel 1:

```c
ADC1->SQR1 &= ~(0xFU << 0);     /* L = 0: sequence length of 1 */
ADC1->SQR1 &= ~(0x1FU << 6);
ADC1->SQR1 |=  (1U << 6);       /* SQ1 = channel 1 (PA0)       */
```

Initialization order:

```text
Clocks
  │
  ▼
PA0 analog
  │
  ▼
ADC clock source (CKMODE)
  │
  ▼
Exit deep-power-down ── Enable regulator ── Wait
  │
  ▼
Calibration (ADCAL)
  │
  ▼
Enable (ADEN) ── Wait for ADRDY
  │
  ▼
Sampling time ── Sequence
```

---

# Conversion

A conversion is started by software and its end is detected by polling the **EOC flag** (`ISR` bit 2):

```c
uint16_t ADC1_read(void)
{
    ADC1->CR |= (1U << 2);               /* ADSTART = 1: start conversion */
    while (!(ADC1->ISR & (1U << 2)));    /* wait until EOC = 1            */
    return (uint16_t)ADC1->DR;           /* reading DR clears EOC         */
}
```

---

# Voltage Calculation

With a 12-bit result and a 3.3 V reference:

```text
V (mV) = raw × 3300 / 4095

1 LSB  = 3300 / 4095 ≈ 0.806 mV
```

The calculation uses integer arithmetic only:

```c
uint16_t raw_value = ADC1_read();
uint32_t adc_value = (raw_value * 3300) / 4095;   /* millivolts */
```

The largest intermediate value is `4095 × 3300 = 13 513 500`, which fits in 32 bits, so no floating point is needed.

---

# Application

The main loop takes one measurement every 250 ms and prints it over USART2:

```c
while (1)
{
    uint16_t raw_value = ADC1_read();
    uint32_t adc_value = (raw_value * 3300) / 4095;

    printf("ADC Raw: %4u | Tension: %lu mV (%lu.%02lu V)\r\n",
           raw_value, adc_value, adc_value / 1000, (adc_value % 1000) / 10);

    my_delay_ms(250);
}
```

The project therefore combines:

```text
SysTick
   │
   └── Software timebase (250 ms period)

ADC1
   │
   └── Analog measurement on PA0

USART2
   │
   └── Measurement output
```

---

# Integration Exercise: Potentiometer-Controlled LED Dimmer

This exercise connects three modules: the ADC1 result sets the TIM2 PWM duty cycle, so the potentiometer controls the brightness of the LED on PA5.

```text
Potentiometer
     │
     ▼
PA0 ── ADC1 ── raw (0 to 4095)
                  │
                  ▼
           Scaling to 0..999
                  │
                  ▼
           TIM2->CCR1 ── PWM 1 kHz ── PA5 ── LED
```

### Scaling

The ADC delivers a 12-bit value from 0 to 4095. TIM2 expects a compare value from 0 to 999 in `CCR1`, because `ARR = 999`. The raw value is therefore rescaled:

```text
CCR1 = raw × 999 / 4095
```

```c
uint16_t raw_value = ADC1_read();

TIM2->CCR1 = (raw_value * 999) / 4095;
```

The largest intermediate value is `4095 × 999 = 4 090 905`, which fits in 32 bits, so integer arithmetic is sufficient.

| Potentiometer | Raw value | `CCR1` | Duty cycle |
| ------------- | --------- | ------ | ---------- |
| Minimum       | 0         | 0      | 0 %        |
| Middle        | 2048      | 499    | 49.9 %     |
| Maximum       | 4095      | 999    | 99.9 %     |

The duty cycle is `CCR1 / (ARR + 1)`.

### Modules involved

```text
ADC1 (Module 06)
   │
   └── Reads the potentiometer

TIM2 (Module 04)
   │
   └── Generates the PWM that drives the LED

USART2 (Module 05)
   │
   └── Prints the measurement

SysTick
   │
   └── Sets the loop period
```

---

# Verification

Open a serial terminal on the ST-LINK Virtual COM Port at 115200 baud, 8N1, as in Module 05.

Test points on PA0:

| PA0 connected to        | Expected raw value | Expected voltage    |
| ----------------------- | ------------------ | ------------------- |
| GND                     | about 0            | about 0 mV          |
| 3V3                     | about 4095         | about 3300 mV       |
| Potentiometer wiper     | 0 to 4095          | follows the knob    |

For the potentiometer, connect its two outer pins to 3V3 and GND and its wiper to PA0.

For the dimmer exercise, turn the potentiometer from one end to the other: the LED on PA5 must go from off to full brightness, and the printed value must follow.

**Never apply more than 3.3 V to PA0.**

Expected terminal output at mid-scale:

```text
ADC Raw: 2048 | Tension: 1650 mV (1.65 V)
```

Useful registers to inspect in the debugger:

```text
RCC->AHB2ENR
GPIOA->MODER
ADC12_COMMON->CCR

ADC1->CR
ADC1->ISR
ADC1->SMPR1
ADC1->SQR1
ADC1->DR
```

---

# Key Registers

| Register            | Function                                                                  |
| ------------------- | ------------------------------------------------------------------------- |
| `RCC->AHB2ENR`      | ADC12 clock enable (bit 13), GPIOA clock enable (bit 0)                   |
| `GPIOA->MODER`      | PA0 analog mode                                                           |
| `ADC12_COMMON->CCR` | ADC clock source, `CKMODE` (bits 17:16)                                   |
| `ADC1->CR`          | `ADCAL` (31), `ADCALDIF` (30), `DEEPPWD` (29), `ADVREGEN` (28), `ADSTART` (2), `ADEN` (0) |
| `ADC1->ISR`         | Status flags: `EOC` (bit 2), `ADRDY` (bit 0)                              |
| `ADC1->SMPR1`       | Sampling time, `SMP1` (bits 5:3)                                          |
| `ADC1->SQR1`        | Sequence length `L` (bits 3:0), first conversion `SQ1` (bits 10:6)        |
| `ADC1->DR`          | Conversion result                                                         |
| `TIM2->CCR1`        | PWM duty cycle, written from the scaled ADC result                        |

---

# Engineering Notes

### Control bits and status flags are in different registers

`ADSTART` is bit 2 of `CR` and `EOC` is bit 2 of `ISR`. They share a bit number but not a register. Polling `CR` bit 2 after starting a conversion returns immediately, because that bit was just set by software, and `DR` is then read before the conversion has finished.

### Bits cleared by hardware

`ADCAL` is set by software and cleared by hardware. The correct wait is "while the bit is still 1", the opposite of a status flag such as `ADRDY` or `EOC`.

### The power-up order is mandatory

The ADC ignores `ADSTART` while `ADEN = 0`, and calibration requires the voltage regulator to be running. Skipping a step does not produce an error: the firmware hangs in a wait loop or returns a meaningless value.

### Sampling time and source impedance

The sampling capacitor needs time to charge through the impedance of the signal source. The default sampling time of 2.5 cycles (about 0.16 µs at 16 MHz) is too short for a high-impedance source such as a potentiometer and gives unstable readings. A long sampling time trades conversion speed for accuracy, which is acceptable at four measurements per second.

### Scaling range of the dimmer

With `ARR = 999`, one PWM period has 1000 counter steps. Scaling to 999 gives a maximum duty cycle of 99.9 %, so the output still has a 1 µs low pulse at full scale. Scaling with `raw × 1000 / 4095` instead reaches a true 100 %. The difference is invisible on an LED, but it matters for loads that need a continuous level.

### Current architecture: polling

```text
Application
    │
    ▼
Set ADSTART
    │
    ▼
Check EOC
    │
    ├── 0 → wait
    │
    └── 1 → read DR
```

The CPU waits about 16 µs per conversion. This is simple and acceptable for slow measurements, but it does not scale to fast or periodic sampling.

### Future improvement

```text
Timer trigger
      │
      ▼
ADC conversion
      │
      ▼
EOC interrupt or DMA
      │
      ▼
Sample buffer
```

A timer-triggered ADC with interrupt or DMA transfer gives a precise sampling period and frees the CPU during conversions.

---

# Module Outcome

The project now includes a fourth hardware-driven peripheral:

```text
GPIO / EXTI
     │
     ▼
Interrupt Events

SysTick
     │
     ▼
Software Timebase

TIM2
     │
     ▼
Hardware PWM

USART2
     │
     ▼
Serial Communication

ADC1
     │
     ▼
Analog Measurement
```

The firmware can now read the analog world, in addition to driving outputs and communicating with a host PC.

With the dimmer exercise, an analog input controls a hardware PWM output for the first time: a complete signal chain from input to actuator.

---

# Next Module

## Module 07 — Interrupt-Driven UART & Ring Buffer

Topics:

* USART interrupts
* RX/TX event handling
* Circular buffers
* Non-blocking communication
* Producer/consumer architecture
* UART command interface
* Interaction with SysTick and EXTI
