# Module 04: Hardware PWM Generation using TIM2

This module introduces **hardware PWM generation** using the STM32G474RE general-purpose timer **TIM2**.

The objective is to generate a **1 kHz PWM signal on PA5** without software-driven timing or periodic CPU interrupts.

---

## Table of Contents

* [Objectives](#objectives)
* [Hardware Architecture](#hardware-architecture)
* [GPIO & TIM2 Configuration](#gpio--tim2-configuration)
* [PWM Timing](#pwm-timing)
* [Duty Cycle](#duty-cycle)
* [Verification](#verification)
* [Key Registers](#key-registers)
* [Engineering Notes](#engineering-notes)
* [Next Module](#next-module)

---

# Objectives

* Enable the TIM2 peripheral.
* Configure PA5 as `TIM2_CH1`.
* Generate a 1 kHz PWM signal.
* Configure PWM Mode 1.
* Control duty cycle through `CCR1`.
* Generate the waveform entirely in hardware.
* Eliminate software delays and PWM interrupts.

---

# Hardware Architecture

```text
16 MHz Clock
     │
     ▼
   TIM2
     │
     ├── PSC = 15
     │      ↓
     │    1 MHz counter
     │
     ├── ARR = 999
     │      ↓
     │    1 kHz PWM
     │
     └── CCR1
            ↓
       Duty Cycle
            │
            ▼
       TIM2_CH1
            │
            ▼
      PA5 / Alternate Function
```

PA5 is configured for:

```text
TIM2_CH1 → AF1
```

---

# GPIO & TIM2 Configuration

Enable the required clocks:

```c
RCC->AHB2ENR  |= (1U << 0);  /* GPIOA */
RCC->APB1ENR1 |= (1U << 0);  /* TIM2 */
```

Configure PA5 for Alternate Function mode:

```c
GPIOA->MODER &= ~(3U << (5U * 2U));
GPIOA->MODER |=  (2U << (5U * 2U));

GPIOA->AFR[0] &= ~(0xFU << (5U * 4U));
GPIOA->AFR[0] |=  (1U << (5U * 4U));  /* AF1 = TIM2_CH1 */
```

---

# PWM Timing

With a 16 MHz timer clock:

```text
Counter frequency:

16 MHz / (15 + 1) = 1 MHz
```

Therefore each timer count represents:

```text
1 µs
```

With:

```c
TIM2->PSC = 15U;
TIM2->ARR = 999U;
```

the PWM frequency is:

```text
1 MHz / (999 + 1) = 1 kHz
```

The PWM period is therefore:

```text
1 ms
```

---

# Duty Cycle

The duty cycle is controlled by `CCR1`.

For example:

```c
TIM2->CCR1 = 250U;
```

With:

```text
ARR = 999
CCR1 = 250
```

the duty cycle is approximately:

```text
25%
```

Typical values:

| `CCR1` | Approx. Duty |
| -----: | -----------: |
|    `0` |           0% |
|  `250` |          25% |
|  `500` |          50% |
|  `750` |          75% |
|  `999` |        ~100% |

---

# PWM Channel Configuration

Configure Channel 1 for PWM Mode 1:

```c
TIM2->CCMR1 &= ~(0x7U << 4);
TIM2->CCMR1 |=  (0x6U << 4);  /* OC1M = PWM Mode 1 */

TIM2->CCMR1 |= (1U << 3);      /* OC1PE */
TIM2->CCER  |= (1U << 0);      /* CC1E */
```

Set the initial duty cycle:

```c
TIM2->CCR1 = 250U;
```

Start the timer:

```c
TIM2->CR1 |= (1U << 0);        /* CEN */
```

---

# Verification

The PWM output can be verified using an oscilloscope or logic analyzer.

Expected signal:

```text
Frequency:   1 kHz
Period:      1 ms
Duty cycle:  ~25%
Output:      PA5 / TIM2_CH1
```

The key registers to inspect are:

```text
RCC->AHB2ENR
RCC->APB1ENR1
GPIOA->MODER
GPIOA->AFR[0]

TIM2->PSC
TIM2->ARR
TIM2->CCR1
TIM2->CCMR1
TIM2->CCER
TIM2->CR1
```

---

# Key Registers

| Register        | Function                    |
| --------------- | --------------------------- |
| `RCC->AHB2ENR`  | GPIOA clock enable          |
| `RCC->APB1ENR1` | TIM2 clock enable           |
| `GPIOA->MODER`  | PA5 Alternate Function mode |
| `GPIOA->AFR[0]` | PA5 → TIM2_CH1 / AF1        |
| `TIM2->PSC`     | Timer prescaler             |
| `TIM2->ARR`     | PWM period                  |
| `TIM2->CCR1`    | PWM duty cycle              |
| `TIM2->CCMR1`   | PWM mode configuration      |
| `TIM2->CCER`    | Channel output enable       |
| `TIM2->CR1`     | Timer enable                |

---

# Engineering Notes

### Hardware-generated waveform

Once configured, TIM2 continuously generates the PWM signal without requiring:

```text
CPU polling
CPU delays
PWM interrupts
software GPIO toggling
```

The CPU is therefore free to execute other application code.

### Deterministic output

The waveform timing is controlled by the timer peripheral rather than software execution timing.

This is an important step toward the deterministic architecture required for:

* motor control
* power electronics
* industrial automation
* embedded control systems

---

# Module Outcome

The architecture has evolved from software-controlled timing to autonomous hardware timing:

```text
Module 03
SysTick
   │
   ▼
Software Timebase
```

to:

```text
Module 04
TIM2
   │
   ▼
Hardware Counter
   │
   ▼
PWM Output
```

The STM32 can now generate a continuous PWM waveform while the CPU performs other tasks.

---

# Next Module

## Module 05 — UART Communication

Topics:

* USART configuration
* Baud-rate calculation
* TX/RX
* Register-level communication
* Polling vs interrupt-driven UART
* Non-blocking communication
* Ring buffers
