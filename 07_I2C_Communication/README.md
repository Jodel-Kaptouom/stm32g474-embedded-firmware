# Module 07: Bare-Metal I2C Communication and HD44780 LCD Display

This module implements a **Register-level I2C1 master driver** on the STM32G474RE, without the STM32 HAL I2C driver, and builds an **HD44780 character LCD driver** on top of it.

The module is built in three steps:

1. **I2C bus**: configure I2C1 on PB8/PB9 and write single bytes, with timeouts and status codes.
2. **LCD protocol**: drive a 16×2 HD44780 display through a PCF8574 I/O expander in 4-bit mode.
3. **Application**: display the potentiometer voltage measured by the ADC from Module 06, in real time.

Every transfer returns a status code, so a missing display is reported over USART2 instead of freezing the firmware.

---

## Table of Contents

* [Objectives](#objectives)
* [Hardware Configuration](#hardware-configuration)
* [Project Structure](#project-structure)
* [Step 1 — I2C Bus](#step-1--i2c-bus)
  * [I2C1 Configuration](#i2c1-configuration)
  * [Timing Configuration](#timing-configuration)
  * [Write Transfer](#write-transfer)
  * [Bus Scan](#bus-scan)
* [Step 2 — LCD Protocol](#step-2--lcd-protocol)
  * [PCF8574 Pin Mapping](#pcf8574-pin-mapping)
  * [4-Bit Mode and the Enable Pulse](#4-bit-mode-and-the-enable-pulse)
  * [Initialization Sequence](#initialization-sequence)
  * [Cursor Position](#cursor-position)
  * [LCD Driver Layers](#lcd-driver-layers)
* [Step 3 — Application](#step-3--application)
* [Demo](#demo)
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
* Drive an HD44780 LCD in **4-bit mode** through a PCF8574 I/O expander.
* Implement the LCD initialization sequence and respect the command execution times.
* Build the LCD driver as **separate layers** on top of the I2C driver.
* Display the live ADC voltage on the LCD.

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
| I/O expander    | PCF8574, 7-bit address `0x27`                      |
| Display         | HD44780 character LCD, 16 columns × 2 lines        |
| Analog input    | Potentiometer on `PA0` (ADC1, Module 06)           |

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
i2c.c  / i2c.h    I2C1 bus driver        (Step 1)
lcd.c  / lcd.h    HD44780 LCD driver     (Step 2)
adc.c  / adc.h    ADC1 driver            (Module 06)
uart.c / uart.h   USART2 driver          (Module 05)
pwm.c  / pwm.h    TIM2 PWM driver        (Module 04)
main.c            Application            (Step 3)
```

Layers:

```text
main.c ── application
   │
lcd.c  ── LCD protocol: never touches a register
   │
i2c.c  ── I2C bus: the only file that touches I2C1
   │
STM32 registers
```

I2C driver interface (`i2c.h`):

```c
#define I2C_OK            0
#define I2C_ERR_TIMEOUT  -1
#define I2C_ERR_NACK     -2

void I2C1_Init(void);
int  I2C1_WriteByte(uint8_t slave_addr, uint8_t data);
int  I2C1_Probe(uint8_t slave_addr);
```

LCD driver interface (`lcd.h`):

```c
#define LCD_I2C_ADDR   0x27

int  LCD_Init(void);
void LCD_Clear(void);
void LCD_SetCursor(uint8_t row, uint8_t col);
void LCD_Print(const char *str);
```

---

# Step 1 — I2C Bus

## I2C1 Configuration

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

## Timing Configuration

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

## Write Transfer

The STM32G4 I2C peripheral handles the start condition, the address and the stop condition in hardware. The firmware only describes the transfer in `CR2` and feeds the data.

Frame on the bus:

```text
 START │ ADDR (7 bits) │ W │ ACK │ DATA (8 bits) │ ACK │ STOP
       │      0x27     │ 0 │     │     0x08      │     │
```

One complete `I2C1_WriteByte()` call sends about 19 bits (address byte, data byte, two ACKs, START and STOP), so it takes about **0.2 ms** at 100 kHz.

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

## Bus Scan

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

# Step 2 — LCD Protocol

## PCF8574 Pin Mapping

The PCF8574 does not understand the display. The byte written to it sets its eight output pins P0 to P7 directly, and these pins are wired to the LCD:

| PCF8574 pin | Bit mask | LCD signal                                 |
| ----------- | -------- | ------------------------------------------ |
| P0          | `0x01`   | RS: 0 = command, 1 = character             |
| P1          | `0x02`   | RW: always 0 (write)                       |
| P2          | `0x04`   | EN: enable, data latched on falling edge   |
| P3          | `0x08`   | Backlight                                  |
| P4 to P7    | `0xF0`   | D4 to D7 (data)                            |

In `lcd.c`, these signals are defined as **bit masks**, not as 0/1 values:

```c
#define LCD_RS   0x01U
#define LCD_RW   0x02U
#define LCD_EN   0x04U
#define LCD_BL   0x08U
```

## 4-Bit Mode and the Enable Pulse

The HD44780 has eight data lines, but the backpack only wires D4 to D7. Every byte, command or character, is therefore sent as **two halves (nibbles)**: the upper four bits first, then the lower four.

The LCD reads the data lines on the **falling edge of EN**. Each nibble therefore needs two I2C writes: the same bits first with EN = 1, then with EN = 0.

Example: character `'A'` = `0x41`, RS = 1, backlight on:

| Byte   | D7–D4 | BL | EN | RW | RS | Role                               |
| ------ | ----- | -- | -- | -- | -- | ---------------------------------- |
| `0x4D` | 0100  | 1  | 1  | 0  | 1  | Upper nibble presented             |
| `0x49` | 0100  | 1  | 0  | 0  | 1  | Falling edge: LCD reads `0x4`      |
| `0x1D` | 0001  | 1  | 1  | 0  | 1  | Lower nibble presented             |
| `0x19` | 0001  | 1  | 0  | 0  | 1  | Falling edge: LCD reads `0x1`      |

One character = 4 I2C writes ≈ 0.8 ms. The backlight bit must be present in **every** byte, otherwise each write switches the backlight off.

## Initialization Sequence

At power-up, the LCD does not know whether the interface is 8-bit or 4-bit. The sequence from the HD44780 datasheet (4-bit interface) forces a known state:

```text
Wait > 40 ms after power-up
    │
    ▼
Nibble 0x3 ── wait > 4.1 ms
Nibble 0x3 ── wait > 100 µs
Nibble 0x3 ── wait
    │
    ▼
Nibble 0x2 ── LCD switches to 4-bit mode
    │
    ▼
Full commands:
  0x28  Function Set   4-bit, 2 lines, 5×8 font
  0x08  Display Off
  0x01  Clear Display  ── wait > 1.52 ms
  0x06  Entry Mode     cursor moves right, no shift
  0x0C  Display On     cursor and blink off
```

Command execution times:

| Command                     | Execution time | Delay needed over I2C?                   |
| --------------------------- | -------------- | ---------------------------------------- |
| Clear Display, Return Home  | 1.52 ms        | Yes: longer than the next I2C writes     |
| All other commands          | 37 µs          | No: one I2C write already takes ~0.2 ms  |

## Cursor Position

Each character cell has an address in the LCD display memory (DDRAM). On a 16×2 display, line 0 starts at `0x00` and line 1 at `0x40`. The Set DDRAM command is `0x80 | address`:

| Position          | Address | Command |
| ----------------- | ------- | ------- |
| Line 0, column 0  | `0x00`  | `0x80`  |
| Line 1, column 0  | `0x40`  | `0xC0`  |
| Line 1, column 5  | `0x45`  | `0xC5`  |

## LCD Driver Layers

The driver is built from the bottom up. Each function only calls the one below it, and the lower layers are `static`, so `main.c` can only reach the four public functions:

```text
LCD_Init / LCD_Clear / LCD_SetCursor / LCD_Print     public  (lcd.h)
                    │
        LCD_Command / LCD_Data                        static
                    │
              LCD_SendByte    two nibbles             static
                    │
             LCD_SendNibble   EN = 1, then EN = 0     static
                    │
             I2C1_WriteByte                           i2c.c
```

Lowest layer:

```c
static int LCD_SendNibble(uint8_t nibble, uint8_t mode)
{
    uint8_t data = (nibble & 0xF0U) | mode | LCD_BL;
    int status;

    status = I2C1_WriteByte(LCD_I2C_ADDR, data | LCD_EN);           /* EN = 1 */
    if (status != I2C_OK)
    {
        return status;
    }
    return I2C1_WriteByte(LCD_I2C_ADDR, data & (uint8_t)~LCD_EN);   /* EN = 0 */
}
```

Error propagation: every layer follows the same rule, "if the call below failed, stop and return its error". The status code travels from `I2C1_WriteByte()` up to `LCD_Init()`, which returns it to `main`:

```text
I2C1_WriteByte → LCD_SendNibble → LCD_SendByte → LCD_Command → LCD_Init → main
```

`LCD_Init()` checks the first write of the sequence: if the display is absent, it fails immediately and the initialization stops there.

---

# Step 3 — Application

Initialization:

```c
I2C1_Init();

if (LCD_Init() != I2C_OK)
{
    printf("Erreur : ecran LCD absent\r\n");
}

LCD_SetCursor(0, 0);
LCD_Print("STM32G4 ADC");
```

Main loop: the ADC/PWM dimmer from Module 06, with the voltage also shown on line 1 of the LCD:

```c
char buf[17];   /* 16 characters + '\0' */

while (1)
{
    uint16_t raw_value = ADC1_read();
    uint32_t adc_value = (raw_value * 3300) / 4095;
    uint32_t pwm_value = (raw_value * 999) / 4095;
    TIM2->CCR1 = pwm_value;

    printf("ADC Raw: %4u | Tension: %lu mV (%lu.%02lu V) | PWM value: %lu\r\n",
           raw_value, adc_value, adc_value / 1000, (adc_value % 1000) / 10, pwm_value);

    snprintf(buf, sizeof(buf), "V:%lu.%02luV RAW:%4u",
             adc_value / 1000, (adc_value % 1000) / 10, raw_value);

    LCD_SetCursor(1, 0);
    LCD_Print(buf);
    my_delay_ms(200);
}
```

`snprintf` writes at most `sizeof(buf)` characters including the final `'\0'`, so the text can never overflow the buffer or the 16 columns.

The project therefore combines:

```text
SysTick ── Software timebase (200 ms period)
ADC1    ── Potentiometer measurement
TIM2    ── PWM LED dimmer
USART2  ── Measurement and error output
I2C1    ── Bus to the LCD backpack
LCD     ── Live voltage display
```

---

# Demo

[Live ADC voltage on the LCD](media/demo_lcd_adc.mp4)

Turning the potentiometer changes the voltage on line 1 of the display in real time. Example frame: `V:1.51V RAW:1880`, consistent with 1880 × 3300 / 4095 = 1515 mV.

---

# Verification

1. Wire the module as shown in [Hardware Configuration](#hardware-configuration).
2. Open a serial terminal on the ST-LINK Virtual COM Port at 115200 baud, 8N1.
3. Run the bus scan once after `I2C1_Init()` to confirm the device address.
4. Run the application.

Tests:

| Test | Action                                    | Expected result                                          |
| ---- | ----------------------------------------- | -------------------------------------------------------- |
| A    | `LCD_Init()` only                         | Clean, empty display, no row of black boxes              |
| B    | Print text on line 0, column 0            | Text appears                                             |
| C    | Set the cursor to line 1, column 5        | Text starts at the right cell                            |
| D    | `LCD_Clear()` after some text             | Display is cleared                                       |
| E    | Module disconnected at start-up           | `Erreur : ecran LCD absent` on the terminal, firmware keeps running |
| F    | Turn the potentiometer                    | Line 1 follows the terminal values in real time          |

Observed result: line 0 shows `STM32G4 ADC`, line 1 shows the live voltage and raw ADC value, and the terminal values match the display.

Troubleshooting:

| Symptom                                     | Likely cause                                                           |
| ------------------------------------------- | ---------------------------------------------------------------------- |
| Firmware freezes in the first transfer      | Wait loop without timeout, pins not on AF4                             |
| `I2C_ERR_NACK`                              | Wrong address (`0x27` vs `0x3F`), SDA/SCL swapped, no common ground   |
| `I2C_ERR_TIMEOUT` on `BUSY`                 | A line is held low: missing pull-ups or a slave stuck mid-transfer     |
| Row of black boxes on line 0                | LCD powered but not initialized                                        |
| Backlight on, nothing visible               | Contrast potentiometer on the backpack, or missing Clear delay         |
| Random or shifted characters                | Nibble pairs out of sync: a nibble was lost while the LCD was busy     |
| Last character of a line missing            | Text longer than 16 characters                                         |

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

With `AUTOEND = 1`, the hardware sends a STOP after a NACK and sets `STOPF`. Clearing only `NACKF` leaves `STOPF` set. The next transfer then finds a stale `STOPF`, returns `I2C_OK` before its own STOP, and the real STOP sets the flag again for the transfer after: every transfer ends too early from then on, and a NACK on the data byte is never reported. Both flags are cleared together.

### `ICR` is write-only

`ICR` reads as 0, so `ICR |= x` happens to work, but it hides the intent. On a register whose flags are both readable and cleared by writing 1, a read-modify-write would clear every flag currently set. The driver writes `ICR = x`, which clears exactly the requested flags. The access type of each bit (`rw`, `r`, `w`, `rc_w1`…) is given in the reference manual.

### Address shift

The 7-bit address is placed in bits 7:1 of `SADD`, hence `slave_addr << 1`. Datasheets sometimes give the 8-bit form (`0x4E` instead of `0x27`): using it unshifted is a classic cause of NACK.

### A missing delay after Clear desynchronizes the LCD

Clear Display keeps the LCD busy for 1.52 ms, and a busy LCD ignores what it receives. Without a delay, the following commands arrived at about 0.4, 0.8, 1.2 and 1.6 ms. The first three nibbles were ignored, but the last one arrived after the LCD was free again. The LCD took this **second** half of Display On as the **first** half of a new command, and from then on paired every nibble with the wrong partner: the display stayed blank or showed garbage.

In 4-bit mode, a single lost nibble shifts every following command by half a byte.

### `my_delay_ms(N)` lasts between N−1 and N ms

The delay counts SysTick interrupts. If an interrupt arrives just after the delay starts, the first "millisecond" is almost zero. `my_delay_ms(2)` can therefore last only about 1 ms, which is shorter than the 1.52 ms Clear time. Rule: to wait at least T ms, request T + 1. This is why the driver uses 3 ms after Clear and 6 ms for the "> 4.1 ms" step.

### A function from another file needs a prototype

`my_delay_ms()` is defined in `main.c`. The compiler handles each `.c` file separately, so `lcd.c` only knows it through its prototype:

```c
void my_delay_ms(uint32_t delay);
```

The linker then finds the actual function in `main.o`. A cleaner design for later: move SysTick and the delay into their own `delay.c` / `delay.h`.

### Layered drivers

`i2c.c` knows how to talk on the bus and nothing about the display. `lcd.c` knows how to talk to the display and never touches a register. The I2C driver can be reused unchanged for a sensor or an EEPROM on the same bus.

### Fixed-width text instead of Clear

Line 1 is always rewritten with exactly 16 characters (`%4u` pads the raw value), so new text overwrites the old one completely. Calling `LCD_Clear()` in the loop is not needed: it would add 1.52 ms per refresh and make the display flicker.

### Current architecture: polling

The driver waits on status flags, with a timeout. One I2C write takes about 0.2 ms, so a full 16-character line takes about 13 ms during which the CPU only waits. This is acceptable at five refreshes per second. Faster or larger displays are better handled by interrupts or DMA.

---

# Module Outcome

The project now includes a fifth hardware-driven peripheral, its first bus shared with external devices, and its first driver built on top of another driver:

```text
GPIO / EXTI ──── Interrupt events
SysTick     ──── Software timebase
TIM2        ──── Hardware PWM
USART2      ──── Serial communication
ADC1        ──── Analog measurement
I2C1        ──── Inter-chip communication
LCD         ──── Local display (on top of I2C1)
```

The firmware no longer only talks to its own peripherals and to a PC: it drives external chips, displays its measurements locally, and handles a missing display without freezing.

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
