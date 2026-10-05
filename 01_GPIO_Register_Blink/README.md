# Module 01: GPIO & Bare-Metal Register Manipulation

## 1. Objectives & Engineering Scope

This module establishes the foundation for **bare-metal firmware development** on the STM32G474RE.

The implementation uses **CMSIS device definitions and direct memory-mapped register access**, without relying on STM32 HAL GPIO APIs.

The main objectives are:

* Implement peripheral initialization through direct register access.
* Understand peripheral clock gating through the **RCC** module.
* Configure GPIO operating modes using bit-field manipulation of `GPIOx->MODER`.
* Control GPIO outputs through `GPIOx->BSRR`.
* Understand the difference between read-modify-write access to `GPIOx->ODR` and the atomic set/reset mechanism provided by `GPIOx->BSRR`.
* Read digital input states through `GPIOx->IDR`.
* Apply bit masking to access individual GPIO pins without modifying unrelated register fields.
* Verify the resulting firmware behavior directly on the target hardware.

The module deliberately starts with a simple LED/button application in order to focus on the underlying **microcontroller architecture and register-level behavior**.

---

# 2. Target Hardware

## 2.1 Microcontroller

**STM32G474RET6**

* Arm® Cortex®-M4
* Floating-Point Unit (FPU)
* DSP instructions
* Up to 170 MHz CPU frequency
* Memory-mapped peripheral architecture

## 2.2 Development Board

**NUCLEO-G474RE**

The first experiment uses the onboard user LED and user push-button.

---

# 3. Hardware Mapping

| Peripheral / Interface |   Pin  | Target Configuration   | Electrical Behavior     |
| :--------------------- | :----: | :--------------------- | :---------------------- |
| **User LED (LD2)**     |  `PA5` | GPIO Output, Push-Pull | LED control output      |
| **User Button (B1)**   | `PC13` | GPIO Digital Input     | Active-LOW button input |

> The exact electrical behavior of the onboard LED and push-button should be verified against the NUCLEO-G474RE schematic when modifying the hardware configuration.

### GPIO assignment

```text
                STM32G474RE
             ┌───────────────┐
             │               │
     B1 ────►│ PC13          │
   Button    │    GPIOC      │
             │               │
             │               │
     LD2 ◄───│ PA5           │
     LED     │    GPIOA      │
             │               │
             └───────────────┘
```

---

# 4. Peripheral Architecture

The firmware configuration follows the STM32 peripheral initialization sequence:

```text
RCC Clock Enable
       │
       ▼
GPIO Mode Configuration
       │
       ├──────────────► PA5 → Output
       │
       └──────────────► PC13 → Input
       │
       ▼
GPIO Register Access
       │
       ├── IDR  → Read button state
       │
       └── BSRR → Control LED
```

The important principle is that the GPIO peripheral cannot be treated independently from the MCU clock tree. The peripheral clock must first be enabled before the GPIO registers are configured or used.

---

# 5. RCC Clock Gating

## 5.1 AHB2 GPIO Clock

GPIOA and GPIOC are connected to the STM32G4 **AHB2 peripheral bus**.

Their clocks are enabled through:

```c
RCC->AHB2ENR
```

The corresponding enable bits are:

* `GPIOAEN` → bit 0
* `GPIOCEN` → bit 2

Example:

```c
RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
RCC->AHB2ENR |= RCC_AHB2ENR_GPIOCEN;
```

The symbolic CMSIS definitions are preferred over hard-coded bit positions because they document the intended hardware function directly.

Equivalent explicit bit manipulation:

```c
RCC->AHB2ENR |= (1U << 0);  // GPIOA clock
RCC->AHB2ENR |= (1U << 2);  // GPIOC clock
```

The CMSIS form is preferred in the final implementation.

---

## 5.2 Why Clock Gating Matters

STM32 peripherals are connected to specific clock domains. Peripheral clocks can be disabled when the corresponding hardware block is not required.

This provides two important architectural properties:

1. **Power management**
   Disabling unused peripheral clocks reduces unnecessary dynamic switching activity.

2. **Controlled peripheral access**
   Firmware must explicitly enable the peripheral clock before configuring and using the peripheral.

The initialization order is therefore:

```text
RCC
 ↓
Enable GPIO clock
 ↓
Configure GPIO registers
 ↓
Use GPIO peripheral
```

---

# 6. GPIO Mode Configuration

Each GPIO pin contains a two-bit field in the `GPIOx->MODER` register.

For GPIO pin `n`:

```text
MODER[2n+1 : 2n]
```

The STM32 GPIO mode encoding is:

| `MODER` value | Mode                   |
| :-----------: | :--------------------- |
|      `00`     | Input                  |
|      `01`     | General-purpose output |
|      `10`     | Alternate function     |
|      `11`     | Analog                 |

---

## 6.1 Configure PA5 as Output

PA5 corresponds to GPIO pin number 5.

Its `MODER` field occupies:

```text
MODER[11:10]
```

The field is first cleared:

```c
GPIOA->MODER &= ~(3U << (5U * 2U));
```

Then output mode is selected:

```c
GPIOA->MODER |= (1U << (5U * 2U));
```

Combined:

```c
GPIOA->MODER &= ~(3U << 10);
GPIOA->MODER |=  (1U << 10);
```

The first operation ensures that the previous configuration is removed without modifying the other GPIO pins.

---

## 6.2 Configure PC13 as Input

PC13 corresponds to GPIO pin number 13.

Its mode field occupies:

```text
MODER[27:26]
```

Input mode corresponds to `00`.

Therefore the field is cleared:

```c
GPIOC->MODER &= ~(3U << (13U * 2U));
```

No additional value is required because:

```text
MODER = 00 → Input
```

---

# 7. GPIO Output Configuration

For PA5, the GPIO output type and speed can also be configured.

## 7.1 Output Type

The `OTYPER` register determines whether the output is:

* Push-pull
* Open-drain

For push-pull operation:

```c
GPIOA->OTYPER &= ~(1U << 5U);
```

---

## 7.2 Output Speed

The GPIO speed is configured through `OSPEEDR`.

For this low-frequency LED application, high GPIO speed is unnecessary.

The speed field for PA5 is cleared:

```c
GPIOA->OSPEEDR &= ~(3U << (5U * 2U));
```

This selects the lowest GPIO output speed setting.

Using the lowest adequate speed is preferable for a simple LED output because it avoids unnecessarily fast output transitions and reduces switching activity.

---

# 8. GPIO Output Control — BSRR

The GPIO output data register is:

```c
GPIOA->ODR
```

However, this module uses:

```c
GPIOA->BSRR
```

for output state changes.

The `BSRR` register provides separate bit fields for:

```text
BSRR[15:0]   → Set corresponding GPIO output
BSRR[31:16]  → Reset corresponding GPIO output
```

For PA5:

### Set PA5

```c
GPIOA->BSRR = (1U << 5U);
```

### Reset PA5

```c
GPIOA->BSRR = (1U << (5U + 16U));
```

The advantage is that the operation directly requests the desired set/reset action without requiring a read-modify-write sequence on `ODR`.

---

# 9. ODR vs BSRR

A typical `ODR` operation may be written as:

```c
GPIOA->ODR |= (1U << 5U);
```

Conceptually, this involves:

```text
Read ODR
   ↓
Modify bit 5
   ↓
Write ODR
```

This is a **read-modify-write (RMW)** operation.

In an environment where the same register can be modified by another execution context, an RMW sequence can introduce concurrency problems.

For example:

```text
Main Context                  Interrupt Context
     │                              │
     │ Read ODR                    │
     │                              │
     │                              ├── Modify ODR
     │                              │
     ├── Modify old value           │
     │                              │
     └── Write ODR                  │
```

An update from another execution context can potentially be lost.

`BSRR` avoids this class of software RMW operation by providing dedicated hardware set/reset semantics:

```c
GPIOA->BSRR = (1U << 5U);          // Set PA5
GPIOA->BSRR = (1U << (5U + 16U));  // Reset PA5
```

This is particularly useful for GPIO control in interrupt-driven firmware.

> The term **atomic** here refers to the hardware set/reset operation exposed by `BSRR`; the exact CPU/bus execution latency should not be interpreted as necessarily being one CPU cycle.

---

# 10. GPIO Input Sampling — IDR

The GPIO input data register is:

```c
GPIOC->IDR
```

For PC13, bit 13 contains the digital input state.

The state can be isolated using:

```c
uint32_t button_state = GPIOC->IDR & (1U << 13U);
```

The result is either:

```text
0 → PC13 is LOW
1 → PC13 is HIGH
```

The bit mask ensures that only the state of PC13 is evaluated.

---

# 11. Active-LOW Button Logic

The onboard button is treated as an **active-LOW input**.

Conceptually:

```text
Button released
       │
       ▼
     PC13 = HIGH
       
Button pressed
       │
       ▼
     PC13 = LOW
```

Therefore the application logic can be expressed as:

```c
if ((GPIOC->IDR & (1U << 13U)) == 0U)
{
    // Button pressed
}
else
{
    // Button released
}
```

The important point is that the logical state of the button is not necessarily identical to the electrical level of the GPIO pin.

---

# 12. Firmware Initialization Sequence

The complete GPIO initialization follows this sequence:

```text
1. Enable GPIOA clock
2. Enable GPIOC clock
3. Configure PA5 as output
4. Configure PA5 output type
5. Configure PA5 output speed
6. Configure PC13 as input
7. Enter application loop
```

Example:

```c
RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
RCC->AHB2ENR |= RCC_AHB2ENR_GPIOCEN;

/* PA5: Output */
GPIOA->MODER &= ~(3U << (5U * 2U));
GPIOA->MODER |=  (1U << (5U * 2U));

/* PA5: Push-pull */
GPIOA->OTYPER &= ~(1U << 5U);

/* PA5: Low speed */
GPIOA->OSPEEDR &= ~(3U << (5U * 2U));

/* PC13: Input */
GPIOC->MODER &= ~(3U << (13U * 2U));
```

---

# 13. Application Logic

The application continuously evaluates the state of PC13 and controls PA5 accordingly.

Conceptually:

```c
while (1)
{
    if ((GPIOC->IDR & (1U << 13U)) == 0U)
    {
        GPIOA->BSRR = (1U << 5U);
    }
    else
    {
        GPIOA->BSRR = (1U << (5U + 16U));
    }
}
```

This implementation is intentionally **polling-based**.

No:

* HAL delay
* software debounce delay
* interrupt
* RTOS
* timer callback

is introduced in Module 01.

The objective is to keep the execution path simple enough to observe and understand the underlying GPIO hardware.

---

# 14. Register-Level Summary

| Register         | Purpose                      | Relevant Bits        |
| :--------------- | :--------------------------- | :------------------- |
| `RCC->AHB2ENR`   | GPIO peripheral clock enable | `GPIOAEN`, `GPIOCEN` |
| `GPIOA->MODER`   | PA5 mode configuration       | Bits `[11:10]`       |
| `GPIOA->OTYPER`  | PA5 output type              | Bit `5`              |
| `GPIOA->OSPEEDR` | PA5 output speed             | Bits `[11:10]`       |
| `GPIOA->BSRR`    | PA5 atomic set/reset control | Bits `5` / `21`      |
| `GPIOC->MODER`   | PC13 mode configuration      | Bits `[27:26]`       |
| `GPIOC->IDR`     | PC13 input state             | Bit `13`             |

---

# 15. Verification

The firmware was deployed to a **NUCLEO-G474RE** target board.

## Expected Behavior

### Button released

```text
PC13 = HIGH
    ↓
LED output = OFF
```

### Button pressed

```text
PC13 = LOW
    ↓
LED output = ON
```

The GPIO configuration and behavior are verified directly on the target hardware.

---

## Debugger Verification

The following registers can be inspected during debugging:

```text
RCC->AHB2ENR
GPIOA->MODER
GPIOA->OTYPER
GPIOA->OSPEEDR
GPIOA->BSRR
GPIOC->MODER
GPIOC->IDR
```

This provides a direct correlation between:

```text
Source Code
    ↓
Register Configuration
    ↓
Peripheral State
    ↓
Physical GPIO Behavior
```

---

# 16. Engineering Considerations

## Polling vs Interrupts

Module 01 intentionally uses polling:

```text
CPU
 │
 ├── Read PC13
 ├── Evaluate state
 ├── Update PA5
 └── Repeat
```

This is simple but inefficient when the CPU has other work to perform.

Module 02 will replace this approach with:

```text
GPIO Event
     │
     ▼
   EXTI
     │
     ▼
   NVIC
     │
     ▼
Interrupt Service Routine
     │
     ▼
Application Event
```

This introduces the concepts of asynchronous hardware events and interrupt-driven firmware.

---

## Blocking Behavior

No software delay is required for the basic GPIO operation.

The main loop continuously executes the input/output decision.

This distinction is important because future modules will introduce explicit timing mechanisms without blocking the processor.

---

# 17. Lessons Learned

This module establishes several fundamental embedded-software principles:

### 1. Peripherals are memory-mapped hardware

Firmware interacts with the GPIO peripheral through defined memory addresses represented by CMSIS register structures.

### 2. Clock configuration is part of peripheral initialization

A GPIO peripheral cannot simply be configured independently of the MCU clock tree.

### 3. Bit masking is fundamental to register programming

Individual fields must be modified without unintentionally changing unrelated configuration bits.

### 4. Hardware-provided atomic operations are preferable to unnecessary software RMW sequences

`BSRR` provides a dedicated mechanism for GPIO set/reset operations.

### 5. Electrical state and logical state are not necessarily identical

An active-LOW button requires the firmware to interpret a LOW GPIO level as the logical `pressed` state.

### 6. Bare-metal programming exposes the actual hardware architecture

Instead of treating GPIO as an abstract API call, the firmware explicitly controls:

```text
Clock
  ↓
Mode
  ↓
Output Configuration
  ↓
Input Sampling
  ↓
Output State
```

---

# 18. Next Module

## Module 02 — EXTI & NVIC Interrupt Handling

The next module will move from polling-based GPIO handling to **hardware-triggered event processing**.

Topics:

* EXTI line configuration
* GPIO-to-EXTI mapping
* Rising/falling edge detection
* Pending interrupt flags
* NVIC configuration
* Interrupt service routines
* Interrupt latency
* Event-driven firmware architecture
* Basic switch debouncing strategy

The objective is to establish the foundation for **asynchronous, interrupt-driven embedded systems**.

---

## Author

**Jodel**
Master's Degree in Electrical and Information Engineering

Focus areas:

* Embedded Systems
* Bare-Metal Firmware
* Real-Time Systems
* Digital Electronics
* Power Electronics
* Industrial Automation
* Medical Technology
