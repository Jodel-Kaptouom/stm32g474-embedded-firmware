# Module 07: Bare-Metal I2C Communication using I2C1

This module implements a **register-level I2C1 master driver** on the STM32G474RE, without the STM32 HAL I2C driver.

The driver writes to a **PCF8574 I/O expander** (the I2C backpack commonly mounted on HD44780 character LCDs) and switches the display backlight on. Every transfer returns a status code, so a missing or silent device is reported over USART2 instead of freezing the firmware.

---

## Table of Contents

* [Objectives](#objectives)
* [Hardware Configuration](#hardware-configuration)
* [Project Structure](#project-structure)
* [I2C1 Configuration](#i2c1-configuration)
* [Timing Configuration](#timing-configuration)
* [Write Transfer](#write-transfer)
* [Bus Scan](#bus-scan)
* [Application](#application)
* [Verification](#verification)
* [Key Registers](#key-registers)
* [Engineering Notes](#engineering-notes)
* [Module Outcome](#module-outcome)
* [Next Module](#next-module)

---

# Objectives

* Configure I2C1 as a bus master using direct register access.
* Configure PB8/PB9 as open-drain alternate-function pins.
* Set the bus timing for **Standard Mode (100 kHz)** from the 16 MHz clock.
* Implement a single-byte write transfer with automatic STOP.
* Detect a missing device through the **NACK** flag.
* Protect every wait loop with a **timeout** so the bus can never freeze the CPU.
* Implement an address-only probe to scan the bus.
* Switch on the backlight of a PCF8574-based LCD backpack.

---

# Hardware Configuration

| Item            | Value                                              |
| --------------- | -------------------------------------------------- |
| Microcontroller | STM32G474RE (ARM Cortex-M4)                        |
| Peripheral      | I2C1 on APB1                                       |
| Kernel clock    | PCLK1 = 16 MHz (HSI), default `I2C1SEL`            |
| SCL             | `PB8`, Alternate Function 4, Arduino header `D15`  |
| SDA             | `PB9`, Alternate Function 4, Arduino header `D14`  |
| Pin type        | Open-drain, internal pull-up enabled               |
| Bus speed       | 100 kHz (Standard Mode)                            |
| Slave           | PCF8574 I/O expander, 7-bit address `0x27`         |

Wiring:

```text
[ Nucleo-G474RE ]                    [ PCF8574 LCD backpack ]

 PB8 / D15 (SCL) ────────────────────── SCL
 PB9 / D14 (SDA) ────────────────────── SDA
 GND ─────────────────────────────────── GND
 3V3 or 5V ───────────────────────────── VCC
```

Bus topology:

```text
        VCC
         │
       ┌─┴─┐  pull-up resistors
       │   │  (on the backpack + internal)
       └─┬─┘
SCL ─────┼──────────┬──────────────
SDA ─────┼──────────┼──────────────
         │          │
   [ STM32 master ] [ PCF8574 slave ]
   open-drain       open-drain
```

Every device can only pull a line **low**. The pull-up resistors bring it back high. This is what allows several devices to share two wires without short circuits.

---

# Project Structure

```text
i2c.c  / i2c.h    I2C1 driver
adc.c  / adc.h    ADC1 driver (Module 06)
uart.c / uart.h   USART2 driver (Module 05)
pwm.c  / pwm.h    TIM2 PWM driver (Module 04)
main.c            Application
```

Driver interface (`i2c.h`):

```c
#define I2C_OK            0
#define I2C_ERR_TIMEOUT  -1
#define I2C_ERR_NACK     -2

void I2C1_Init(void);
int  I2C1_WriteByte(uint8_t slave_addr, uint8_t data);
int  I2C1_Probe(uint8_t slave_addr);
```

---

# I2C1 Configuration

### 1. Clocks

```c
RCC->APB1ENR1 |= (1U << 21);   /* I2C1  */
RCC->AHB2ENR  |= (1U << 1);    /* GPIOB */
```

### 2. PB8/PB9 in Alternate Function mode

```c
GPIOB->MODER &= ~((3U << (8 * 2)) | (3U << (9 * 2)));
GPIOB->MODER |=  ((2U << (8 * 2)) | (2U << (9 * 2)));
```

### 3. Open-drain outputs

```c
GPIOB->OTYPER |= (1U << 8) | (1U << 9);
```

### 4. Pull-up resistors

```c
GPIOB->PUPDR &= ~((3U << (8 * 2)) | (3U << (9 * 2)));
GPIOB->PUPDR |=  ((1U << (8 * 2)) | (1U << (9 * 2)));
```

The internal pull-ups (about 40 kΩ) are weak. They keep the lines defined when nothing is connected, but the bus relies on the stronger external pull-ups of the PCF8574 module.

### 5. AF4 = I2C1

PB8 and PB9 are pins 8 to 15, so they are configured in `AFR[1]` (AFRH):

```c
GPIOB->AFR[1] &= ~((0xFU << 0) | (0xFU << 4));
GPIOB->AFR[1] |=  ((4U   << 0) | (4U   << 4));   /* PB8 → AF4, PB9 → AF4 */
```

### 6. Timing and enable

`TIMINGR` may only be written while the peripheral is disabled:

```c
I2C1->CR1 &= ~(1U << 0);          /* PE = 0 */
I2C1->TIMINGR = 0x00303D5B;       /* 100 kHz @ 16 MHz */
I2C1->CR1 |=  (1U << 0);          /* PE = 1 */
```

---

# Timing Configuration

`TIMINGR = 0x00303D5B` breaks down as follows:

| Field    | Bits  | Value     | Meaning                          |
| -------- | ----- | --------- | -------------------------------- |
| `PRESC`  | 31:28 | 0         | t_PRESC = 1 / 16 MHz = 62.5 ns   |
| `SCLDEL` | 23:20 | 3         | Data setup time                  |
| `SDADEL` | 19:16 | 0         | Data hold time                   |
| `SCLH`   | 15:8  | 0x3D (61) | SCL high period                  |
| `SCLL`   | 7:0   | 0x5B (91) | SCL low period                   |

```text
t_SCLL   = (91 + 1) × 62.5 ns = 5.75 µs
t_SCLH   = (61 + 1) × 62.5 ns = 3.875 µs
t_SCLDEL = (3 + 1)  × 62.5 ns = 250 ns   (≥ 250 ns required in Standard Mode)

Period  ≈ 5.75 + 3.875 µs + sync and rise times ≈ 10 µs   →   ≈ 100 kHz
```

---

# Write Transfer

The STM32G4 I2C peripheral handles the start condition, the address and the stop condition in hardware. The firmware only describes the transfer in `CR2` and feeds the data.

Frame on the bus:

```text
 START │ ADDR (7 bits) │ W │ ACK │ DATA (8 bits) │ ACK │ STOP
       │      0x27     │ 0 │     │     0x08      │     │
```

### 1. Wait for a free bus

```c
while (I2C1->ISR & (1U << 15))        /* BUSY */
{
    if (--t == 0) return I2C_ERR_TIMEOUT;
}
```

### 2. Describe and start the transfer

```c
I2C1->CR2 = ((uint32_t)slave_addr << 1)   /* SADD: 7-bit address in bits 7:1 */
          | (1U << 16)                    /* NBYTES = 1                      */
          | (1U << 25)                    /* AUTOEND: STOP after last byte   */
          | (1U << 13);                   /* START                           */
```

`RD_WRN` (bit 10) stays at 0, which selects a write.

### 3. Wait for TXIS, or detect a NACK

```c
while (!(I2C1->ISR & (1U << 1)))          /* TXIS */
{
    if (I2C1->ISR & (1U << 4))            /* NACKF: no device answered */
    {
        /* With AUTOEND the hardware sends STOP by itself: wait for it */
        while (!(I2C1->ISR & (1U << 5))) { /* with timeout */ }
        I2C1->ICR = (1U << 4) | (1U << 5);
        return I2C_ERR_NACK;
    }
    if (--t == 0) return I2C_ERR_TIMEOUT;
}
```

### 4. Send the data and wait for STOP

```c
I2C1->TXDR = data;

while (!(I2C1->ISR & (1U << 5)))          /* STOPF */
{
    if (--t == 0) return I2C_ERR_TIMEOUT;
}
I2C1->ICR = (1U << 5);
return I2C_OK;
```

Transfer sequence:

```text
BUSY = 0 ?
    │
    ▼
Write CR2 (address, NBYTES, AUTOEND, START)
    │
    ▼
TXIS ? ──── NACKF ──▶ wait STOPF ──▶ clear ──▶ I2C_ERR_NACK
    │
    ▼
Write TXDR
    │
    ▼
STOPF ? ──▶ clear ──▶ I2C_OK
```

---

# Bus Scan

`I2C1_Probe()` sends only the address (`NBYTES = 0`) and checks whether a device acknowledges it. Nothing is written into the device.

```c
I2C1->CR2 = ((uint32_t)slave_addr << 1)
          | (1U << 25)                    /* AUTOEND, NBYTES = 0 */
          | (1U << 13);                   /* START               */

/* wait STOPF, then: ACK if NACKF = 0 */
```

Scanning every valid 7-bit address:

```c
for (uint8_t a = 0x08; a < 0x78; a++)
{
    if (I2C1_Probe(a) == I2C_OK)
        printf("I2C device found at 0x%02X\r\n", a);
}
```

Typical addresses: `0x27` for a PCF8574, `0x3F` for a PCF8574A.

---

# Application

The main loop keeps the ADC/PWM dimmer from Module 06 and adds one I2C write per cycle:

```c
while (1)
{
    uint16_t raw_value = ADC1_read();
    uint32_t adc_value = (raw_value * 3300) / 4095;
    uint32_t pwm_value = (raw_value * 999) / 4095;
    TIM2->CCR1 = pwm_value;

    printf("ADC Raw: %4u | Tension: %lu mV (%lu.%02lu V) | PWM value: %lu\r\n",
           raw_value, adc_value, adc_value / 1000, (adc_value % 1000) / 10, pwm_value);

    my_delay_ms(250);

    if (I2C1_WriteByte(0x27, 0x08) != I2C_OK)
        printf("Erreur I2C\r\n");
}
```

On a PCF8574 LCD backpack, the byte written sets the eight output pins P0 to P7 directly. P3 drives the backlight transistor, so `0x08` (`0000 1000b`) switches the backlight on.

The project therefore combines:

```text
SysTick ── Software timebase (250 ms period)
ADC1    ── Potentiometer measurement
TIM2    ── PWM LED dimmer
USART2  ── Measurement and error output
I2C1    ── LCD backpack control
```

---

# Verification

1. Wire the module as shown in [Hardware Configuration](#hardware-configuration).
2. Open a serial terminal on the ST-LINK Virtual COM Port at 115200 baud, 8N1.
3. Run the bus scan once after `I2C1_Init()` to confirm the device address.
4. Run the main loop.

Observed result:

* The terminal prints the ADC lines every 250 ms and **never prints `Erreur I2C`**: every write to `0x27` is acknowledged.
* The ADC reading stays within ±1 LSB, and the PWM value follows it.
* The LCD backlight is on.

Additional check: alternate `0x00` and `0x08` every 500 ms. The backlight must blink.

Troubleshooting:

| Symptom                                   | Likely cause                                                          |
| ----------------------------------------- | --------------------------------------------------------------------- |
| Firmware freezes in the first transfer    | Wait loop without timeout, pins not on AF4                            |
| `I2C_ERR_NACK`                            | Wrong address (`0x27` vs `0x3F`), SDA/SCL swapped, no common ground  |
| `I2C_ERR_TIMEOUT` on `BUSY`               | A line is held low: missing pull-ups or a slave stuck mid-transfer    |
| Scan finds nothing                        | Wiring, module not powered, pins not configured as open-drain AF4     |

Useful registers to inspect in the debugger:

```text
RCC->APB1ENR1
GPIOB->MODER
GPIOB->OTYPER
GPIOB->PUPDR
GPIOB->AFR[1]

I2C1->CR1
I2C1->CR2
I2C1->TIMINGR
I2C1->ISR
I2C1->TXDR
```

---

# Key Registers

| Register        | Function                                                                    |
| --------------- | --------------------------------------------------------------------------- |
| `RCC->APB1ENR1` | I2C1 clock enable (bit 21)                                                  |
| `RCC->AHB2ENR`  | GPIOB clock enable (bit 1)                                                  |
| `GPIOB->MODER`  | PB8/PB9 Alternate Function mode                                             |
| `GPIOB->OTYPER` | PB8/PB9 open-drain                                                          |
| `GPIOB->PUPDR`  | PB8/PB9 pull-up                                                             |
| `GPIOB->AFR[1]` | AF4 = I2C1 for PB8 (bits 3:0) and PB9 (bits 7:4)                            |
| `I2C1->CR1`     | Peripheral enable `PE` (bit 0)                                              |
| `I2C1->TIMINGR` | SCL timing: `PRESC`, `SCLDEL`, `SDADEL`, `SCLH`, `SCLL`                     |
| `I2C1->CR2`     | `SADD` (9:0), `RD_WRN` (10), `START` (13), `NBYTES` (23:16), `AUTOEND` (25) |
| `I2C1->ISR`     | `TXIS` (1), `NACKF` (4), `STOPF` (5), `BUSY` (15)                           |
| `I2C1->ICR`     | Flag clear: `NACKCF` (4), `STOPCF` (5)                                      |
| `I2C1->TXDR`    | Transmit data register                                                      |

---

# Engineering Notes

### Wrong alternate function = silent bus

PB8 and PB9 must be set to **AF4**. An earlier version wrote `1` instead of `4` in `AFR[1]`, which routed the pins to another peripheral. The I2C1 peripheral then never saw an ACK or a NACK, and the firmware froze waiting for `TXIS`. The comment said AF4, the code did not: always check the value written, not the comment.

### Every wait loop needs a timeout

An I2C bus depends on external hardware: a disconnected module, a missing pull-up or a slave stuck mid-byte can hold a line low indefinitely. Without a timeout, one faulty wire freezes the whole firmware. With status codes, the application keeps running and reports the fault.

### NACK also produces a STOP

With `AUTOEND = 1`, the hardware sends a STOP after a NACK and sets `STOPF`. Clearing only `NACKF` leaves `STOPF` set, and the next transfer then sees a stale STOP and ends too early. Both flags are cleared together.

### `ICR` is write-only

`ICR` reads as 0, so `ICR |= x` happens to work, but it hides the intent. The driver writes `ICR = x`, which clears exactly the requested flags.

### Address shift

The 7-bit address is placed in bits 7:1 of `SADD`, hence `slave_addr << 1`. Datasheets sometimes give the 8-bit form (`0x4E` instead of `0x27`): using it unshifted is a classic cause of NACK.

### Current architecture: polling

The driver waits on status flags, with a timeout. One byte at 100 kHz takes about 0.1 ms, which is acceptable at four transfers per second. Longer transfers, such as writing full text lines to an LCD, are better handled by interrupts or DMA.

---

# Module Outcome

The project now includes a fifth hardware-driven peripheral and its first bus shared with external devices:

```text
GPIO / EXTI ──── Interrupt events
SysTick     ──── Software timebase
TIM2        ──── Hardware PWM
USART2      ──── Serial communication
ADC1        ──── Analog measurement
I2C1        ──── Inter-chip communication
```

The firmware no longer only talks to its own peripherals and to a PC: it now drives external chips, and it handles their absence without freezing.

---

# Next Module

## Module 08 — Interrupt-Driven UART & Ring Buffer

Topics:

* USART interrupts
* RX/TX event handling
* Circular buffers
* Non-blocking communication
* Producer/consumer architecture
* UART command interface
* Interaction with SysTick and EXTI
