# Module 02: External Interrupts & Event Handling

Hardware-driven GPIO event handling on the **STM32G474RE** using **EXTI** and the ARM Cortex-M4 **NVIC**.

This module replaces GPIO polling with an **asynchronous interrupt-driven architecture**.

---

## Table of Contents

* [Objectives](#objectives)
* [Hardware](#hardware)
* [Architecture](#architecture)
* [Implementation](#implementation)

  * [1. GPIO Configuration](#1-gpio-configuration)
  * [2. EXTI Routing](#2-exti-routing)
  * [3. Edge Detection](#3-edge-detection)
  * [4. NVIC Configuration](#4-nvic-configuration)
  * [5. Interrupt Handler](#5-interrupt-handler)
* [Polling vs Interrupts](#polling-vs-interrupts)
* [Verification](#verification)
* [Key Registers](#key-registers)
* [Engineering Notes](#engineering-notes)
* [Next Module](#next-module)

---

# Objectives

The module demonstrates:

* GPIO interrupt configuration using EXTI
* GPIO-to-EXTI routing through SYSCFG
* Rising/falling edge detection
* NVIC interrupt configuration
* Interrupt Service Routine (ISR) design
* EXTI pending-flag handling
* Asynchronous event processing
* Transition from polling to interrupt-driven firmware

The implementation uses **CMSIS and direct register access**, without STM32 HAL GPIO/EXTI APIs.

---

# Hardware

| Function       |        Pin       | Configuration             |
| :------------- | :--------------: | :------------------------ |
| User Button B1 |      `PC13`      | Digital input, active-LOW |
| User LED LD2   |       `PA5`      | Push-pull output          |
| EXTI source    |     `EXTI13`     | Falling-edge interrupt    |
| NVIC channel   | `EXTI15_10_IRQn` | Enabled                   |

### Button logic

```text
Released → PC13 = HIGH
Pressed  → PC13 = LOW
                 │
                 ▼
             Falling Edge
```

---

# Architecture

```text
        B1
        │
       PC13
        │
        ▼
     SYSCFG
        │
   EXTI13 routing
        │
        ▼
      EXTI
        │
   Falling Edge
        │
        ▼
      NVIC
        │
 EXTI15_10_IRQn
        │
        ▼
      Cortex-M4
        │
        ▼
EXTI15_10_IRQHandler()
        │
        ▼
       PA5
        │
        ▼
       LD2
```

The CPU no longer needs to continuously poll `GPIOC->IDR`.

---

# Implementation

## 1. GPIO Configuration

Enable GPIO clocks:

```c
RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
RCC->AHB2ENR |= RCC_AHB2ENR_GPIOCEN;
```

Configure PA5 as output:

```c
GPIOA->MODER &= ~(3U << (5U * 2U));
GPIOA->MODER |=  (1U << (5U * 2U));

GPIOA->OTYPER &= ~(1U << 5U);
```

Configure PC13 as input:

```c
GPIOC->MODER &= ~(3U << (13U * 2U));
```

---

## 2. EXTI Routing

Enable the SYSCFG clock:

```c
RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
```

Route PC13 to EXTI13 through `SYSCFG_EXTICR4`:

```c
SYSCFG->EXTICR[3] &= ~(0x0FU << 4);
SYSCFG->EXTICR[3] |=  (0x02U << 4);
```

Result:

```text
PC13 → EXTI13
```

---

## 3. Edge Detection

Because the button is active-LOW, a button press produces a falling edge.

Configure EXTI13 for falling-edge detection:

```c
EXTI->RTSR1 &= ~(1U << 13);
EXTI->FTSR1 |=  (1U << 13);
```

Enable the EXTI interrupt:

```c
EXTI->IMR1 |= (1U << 13);
```

Clear a possible stale pending flag:

```c
EXTI->PR1 = (1U << 13);
```

`PR1` uses **Write-1-to-Clear (W1C)** semantics.

---

## 4. NVIC Configuration

EXTI lines 10–15 share the same NVIC interrupt channel:

```c
NVIC_SetPriority(EXTI15_10_IRQn, 2U);
NVIC_EnableIRQ(EXTI15_10_IRQn);
```

Architecture:

```text
EXTI10 ─┐
EXTI11 ─┤
EXTI12 ─┤
EXTI13 ─┼──► EXTI15_10_IRQn
EXTI14 ─┤
EXTI15 ─┘
```

The ISR therefore checks which EXTI line generated the event.

---

## 5. Interrupt Handler

Minimal ISR implementation:

```c
void EXTI15_10_IRQHandler(void)
{
    if (EXTI->PR1 & (1U << 13))
    {
        /* Clear pending flag */
        EXTI->PR1 = (1U << 13);

        /* LED response */
        GPIOA->BSRR = (1U << 5U);
    }
}
```

The recommended ISR sequence is:

```text
Check source
    ↓
Clear pending flag
    ↓
Execute minimal response
    ↓
Return
```

Interrupt handlers should remain short and deterministic.

---

# Polling vs Interrupts

### Module 01 — Polling

```text
while(1)
{
    read PC13
    check state
    update LED
}
```

The CPU continuously checks the GPIO.

### Module 02 — Interrupt Driven

```text
PC13
 │
 ▼
EXTI
 │
 ▼
NVIC
 │
 ▼
ISR
 │
 ▼
Application
```

The CPU reacts only when the configured hardware event occurs.

This architecture is more appropriate for asynchronous events and provides the foundation for later interrupt-driven peripherals.

---

# Verification

The module is verified on the **NUCLEO-G474RE**.

### Expected behavior

```text
Button released
    ↓
PC13 = HIGH
    ↓
No interrupt

Button pressed
    ↓
PC13 = LOW
    ↓
Falling edge
    ↓
EXTI13
    ↓
EXTI15_10_IRQHandler()
    ↓
LED response
```

### Debug registers

The following registers can be inspected during debugging:

```text
RCC->AHB2ENR
RCC->APB2ENR
SYSCFG->EXTICR[3]
GPIOC->MODER
EXTI->RTSR1
EXTI->FTSR1
EXTI->IMR1
EXTI->PR1
```

---

# Key Registers

| Register            | Purpose                       |
| :------------------ | :---------------------------- |
| `RCC->AHB2ENR`      | GPIO clock enable             |
| `RCC->APB2ENR`      | SYSCFG clock enable           |
| `SYSCFG->EXTICR[3]` | PC13 → EXTI13 routing         |
| `EXTI->RTSR1`       | Rising-edge selection         |
| `EXTI->FTSR1`       | Falling-edge selection        |
| `EXTI->IMR1`        | Interrupt mask                |
| `EXTI->PR1`         | Pending flag / W1C            |
| `NVIC`              | Interrupt priority and enable |
| `GPIOA->BSRR`       | LED set/reset                 |

---

# Engineering Notes

### Active-LOW input

The button generates:

```text
HIGH → LOW = Press
LOW  → HIGH = Release
```

Therefore the interrupt uses the **falling edge**.

### W1C pending flag

`EXTI->PR1` is not a conventional read/write register.

Clear the pending bit explicitly:

```c
EXTI->PR1 = (1U << 13);
```

### ISR design

Avoid long operations inside the ISR.

For more complex applications, the ISR should acknowledge the hardware event and set an event flag:

```c
volatile uint32_t button_event = 0U;

void EXTI15_10_IRQHandler(void)
{
    if (EXTI->PR1 & (1U << 13))
    {
        EXTI->PR1 = (1U << 13);
        button_event = 1U;
    }
}
```

Application processing can then remain in the main context.

### Debouncing

Mechanical buttons can generate multiple transitions during a single press.

Debouncing is intentionally kept outside the basic EXTI implementation and will be addressed later using timing/state-machine techniques.

---

# Module Outcome

This module establishes the following hardware event chain:

```text
GPIO
 ↓
SYSCFG
 ↓
EXTI
 ↓
NVIC
 ↓
Cortex-M4 ISR
```

The project has now moved from **polling-based GPIO control** to **asynchronous hardware event handling**.

---

# Next Module

## Module 03 — Deterministic Timing with SysTick

Next topics:

* Cortex-M SysTick
* Periodic interrupts
* Millisecond time base
* Non-blocking delays
* Timer rollover
* Task scheduling
* Timing jitter
* Interaction between SysTick and EXTI
