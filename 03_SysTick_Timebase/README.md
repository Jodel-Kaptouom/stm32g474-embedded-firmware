# Module 03: ARM Cortex-M SysTick Timer & Custom Timebase

This module introduces the **ARM Cortex-M4 SysTick timer** to create a deterministic **1 ms software timebase** without using `HAL_Delay()`.

The goal is to replace blocking delays with **non-blocking, tick-based scheduling**.

---

## Table of Contents

* [Objectives](#objectives)
* [Hardware & Architecture](#hardware--architecture)
* [SysTick Configuration](#systick-configuration)
* [1 ms Timebase](#1-ms-timebase)
* [Non-Blocking Timing](#non-blocking-timing)
* [Verification](#verification)
* [Key Registers](#key-registers)
* [Engineering Notes](#engineering-notes)
* [Next Module](#next-module)

---

# Objectives

* Configure the Cortex-M4 SysTick timer.
* Generate a periodic **1 ms interrupt**.
* Maintain a monotonic millisecond counter.
* Replace blocking delays with non-blocking timing.
* Implement simple periodic task scheduling.
* Continue using CMSIS/direct register access rather than `HAL_Delay()`.

---

# Hardware & Architecture

SysTick is integrated into the **Cortex-M4 core**, unlike STM32 peripheral timers such as TIM2 or TIM3.

```text
Cortex-M4
   │
   └── SysTick
        │
        ├── 24-bit down-counter
        ├── HCLK source
        └── SysTick_Handler()
                │
                ▼
             ms_ticks
                │
        ┌───────┼───────┐
        ▼       ▼       ▼
      Task A  Task B  Task C
```

For this module:

```text
CPU Clock = 16 MHz
Tick      = 1 ms
```

---

# SysTick Configuration

For a 1 ms period:

```text
LOAD = (fCPU / 1000) - 1
     = (16,000,000 / 1000) - 1
     = 15,999
     = 0x3E7F
```

Register-level initialization:

```c
void My_SysTick_Init_1ms(void)
{
    SysTick->LOAD = (16000000U / 1000U) - 1U;

    SysTick->VAL = 0U;

    SysTick->CTRL =
          (1U << 2)   /* CLKSOURCE: processor clock */
        | (1U << 1)   /* TICKINT: enable interrupt */
        | (1U << 0);  /* ENABLE */
}
```

The resulting configuration is:

```text
16 MHz HCLK
     │
     ▼
SysTick counter
     │
     ├── 1 ms period
     │
     ▼
SysTick_Handler()
```

---

# 1 ms Timebase

The ISR increments a global tick counter:

```c
volatile uint32_t ms_ticks = 0U;

void SysTick_Handler(void)
{
    ms_ticks++;
}
```

The counter represents elapsed milliseconds since initialization.

Example:

```text
ms_ticks = 0       → startup
ms_ticks = 1000    → approximately 1 second
ms_ticks = 5000    → approximately 5 seconds
```

---

# Non-Blocking Timing

Instead of:

```c
HAL_Delay(500);
```

the application can compare the current tick against the previous execution time:

```c
uint32_t last_run = 0U;

while (1)
{
    if ((ms_ticks - last_run) >= 500U)
    {
        last_run = ms_ticks;

        GPIOA->ODR ^= (1U << 5U);
    }

    /* Other application processing */
}
```

This allows the CPU to continue executing other tasks between scheduled events.

```text
while(1)
   │
   ├── Task A
   │
   ├── Task B
   │
   ├── Check SysTick
   │
   └── Task C
```

No blocking delay is required.

---

# Verification

The module can be verified using STM32CubeIDE debugger tools.

### Check the timebase

Monitor:

```text
ms_ticks
```

It should increase approximately once every millisecond.

### Check registers

Inspect:

```text
SysTick->CTRL
SysTick->LOAD
SysTick->VAL
```

Expected:

```text
LOAD ≈ 15999
CTRL  → counter + interrupt + processor clock enabled
VAL   → continuously counting down
```

For precise timing measurements, use a **logic analyzer or oscilloscope** rather than relying only on debugger timing.

---

# Key Registers

| Register         | Purpose                                    |
| ---------------- | ------------------------------------------ |
| `SysTick->CTRL`  | Clock source, interrupt and counter enable |
| `SysTick->LOAD`  | Reload value                               |
| `SysTick->VAL`   | Current counter value                      |
| `SysTick->CALIB` | Optional calibration information           |

---

# Engineering Notes

### 1. `volatile`

`ms_ticks` is modified inside an ISR and therefore declared:

```c
volatile uint32_t ms_ticks;
```

### 2. Rollover-safe timing

This pattern:

```c
if ((ms_ticks - last_run) >= period)
```

is preferable to comparing absolute timestamps because unsigned arithmetic naturally handles `uint32_t` rollover.

### 3. Keep the ISR short

The SysTick handler should perform minimal work:

```c
void SysTick_Handler(void)
{
    ms_ticks++;
}
```

Application tasks should remain in the main context whenever possible.

### 4. Timing is not CPU-free

SysTick introduces periodic interrupt overhead. The 1 ms tick must therefore remain lightweight, especially as more interrupts are added.

---

# Module Outcome

The firmware now has a basic deterministic software timebase:

```text
SysTick
   │
   ▼
1 ms interrupt
   │
   ▼
ms_ticks
   │
   ▼
Non-blocking scheduling
```

This provides the timing foundation for future asynchronous firmware modules.

---

# Next Module

## Module 04 — Hardware Timers & PWM

The next module will move timing-critical operations from a software timebase to STM32 hardware timers.

Topics:

* TIM2 / TIM3
* Timer prescaler
* Auto-reload
* Hardware counting
* PWM generation
* Duty cycle
* Frequency control
* CPU-independent waveform generation
