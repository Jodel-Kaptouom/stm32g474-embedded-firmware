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
* [Feature Extension: EXTI13 Duty Control](#feature-extension-exti13-duty-control)
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

PA5 is configured as:

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

Therefore:

```text
1 timer tick = 1 µs
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

The PWM period is:

```text
1 ms
```

---

# Duty Cycle

The duty cycle is controlled by `CCR1`.

Example:

```c
TIM2->CCR1 = 250U;
```

With `ARR = 999`, this produces approximately:

```text
25% duty cycle
```

| `CCR1` | Approx. Duty |
| -----: | -----------: |
|    `0` |           0% |
|  `250` |          25% |
|  `500` |          50% |
|  `750` |          75% |
|  `999` |        ~100% |

---

# PWM Channel Configuration

```c
TIM2->CCMR1 &= ~(0x7U << 4);
TIM2->CCMR1 |=  (0x6U << 4);  /* PWM Mode 1 */

TIM2->CCMR1 |= (1U << 3);      /* OC1PE */
TIM2->CCER  |= (1U << 0);      /* CC1E */

TIM2->CCR1 = 250U;

TIM2->CR1 |= (1U << 0);        /* CEN */
```

---

# Verification

Expected output on PA5:

```text
Frequency:   1 kHz
Period:      1 ms
Duty cycle:  ~25%
Channel:     TIM2_CH1
```

Verify with an oscilloscope or logic analyzer.

Useful registers:

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

| Register        | Function               |
| --------------- | ---------------------- |
| `RCC->AHB2ENR`  | GPIOA clock            |
| `RCC->APB1ENR1` | TIM2 clock             |
| `GPIOA->MODER`  | PA5 Alternate Function |
| `GPIOA->AFR[0]` | PA5 → TIM2_CH1         |
| `TIM2->PSC`     | Timer prescaler        |
| `TIM2->ARR`     | PWM period             |
| `TIM2->CCR1`    | Duty cycle             |
| `TIM2->CCMR1`   | PWM mode               |
| `TIM2->CCER`    | Channel enable         |
| `TIM2->CR1`     | Timer enable           |

---

# Engineering Notes

Once configured, TIM2 generates the PWM waveform autonomously.

No software:

* delay loop
* GPIO toggling
* PWM interrupt

is required for continuous waveform generation.

This provides deterministic timing while leaving the CPU available for application processing.

---

# Feature Extension: EXTI13 Duty Control

The PWM output can also be controlled asynchronously using the **EXTI13 button interrupt** introduced in Module 02.

Each button press advances the duty cycle:

```text
0% → 25% → 50% → 75% → ~100% → 0%
```

Architecture:

```text
B1 / PC13
    │
    ▼
  EXTI13
    │
    ▼
   NVIC
    │
    ▼
   ISR
    │
    ▼
TIM2->CCR1
    │
    ▼
 PWM Duty Cycle
```

The important design constraint is that the ISR performs **no blocking delay**.

## ISR Implementation

```c
void EXTI15_10_IRQHandler(void)
{
    if (EXTI->PR1 & (1U << 13))
    {
        /* Clear EXTI13 pending flag (W1C) */
        EXTI->PR1 = (1U << 13);

        /* Persistent duty-cycle state */
        static uint16_t duty = 0U;

        /* 0% → 25% → 50% → 75% → ~100% → 0% */
        duty += 250U;

        if (duty > 1000U)
        {
            duty = 0U;
        }

        /* Limit CCR1 to ARR */
        TIM2->CCR1 = (duty >= 1000U) ? 999U : duty;
    }
}
```

### Design Principles

**Persistent state**

```c
static uint16_t duty;
```

preserves the duty-cycle value between interrupts without requiring a separate global variable.

**No busy-waiting**

The ISR contains no delay loop. This keeps interrupt execution short and avoids blocking other time-critical interrupts such as SysTick.

**Hardware PWM remains autonomous**

The ISR only updates:

```c
TIM2->CCR1
```

TIM2 continues generating the PWM waveform independently of the CPU.

**Register saturation**

Since:

```text
ARR = 999
```

the compare value is limited to:

```text
CCR1 ≤ 999
```

---

# Module Outcome

The module now combines two hardware-driven mechanisms:

```text
             ┌──────────────┐
             │    EXTI13    │
             │   Button B1  │
             └──────┬───────┘
                    │
                    ▼
                  NVIC
                    │
                    ▼
                   ISR
                    │
                    ▼
              TIM2->CCR1
                    │
                    ▼
             Hardware PWM
                    │
                    ▼
                PA5 / LED
```

The **CPU only updates the PWM command when an external event occurs**. The actual 1 kHz waveform continues to be generated by TIM2 hardware.

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
