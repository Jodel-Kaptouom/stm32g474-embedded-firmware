# Module 05: Bare-Metal UART Communication using USART2

This module introduces **register-level UART communication** on the STM32G474RE using **USART2**.

The objective is to transmit serial messages without using the STM32 HAL UART driver.

---

## Table of Contents

* [Objectives](#objectives)
* [Hardware Configuration](#hardware-configuration)
* [USART2 Configuration](#usart2-configuration)
* [Transmission](#transmission)
* [Application](#application)
* [Verification](#verification)
* [Key Registers](#key-registers)
* [Engineering Notes](#engineering-notes)
* [Next Module](#next-module)

---

# Objectives

* Configure USART2 using direct register access.
* Configure PA2/PA3 for UART communication.
* Set the baud rate to approximately **115200 baud**.
* Implement character transmission.
* Implement string transmission.
* Understand the TX polling mechanism.
* Prepare the architecture for future interrupt-driven UART and ring buffers.

---

# Hardware Configuration

USART2 is connected through:

| Function  | Pin   | Configuration        |
| --------- | ----- | -------------------- |
| USART2_TX | `PA2` | Alternate Function 7 |
| USART2_RX | `PA3` | Alternate Function 7 |
| Baud rate | —     | ~115200 baud         |

Architecture:

```text
Application
     │
     ▼
UART2_SendString()
     │
     ▼
UART2_SendChar()
     │
     ▼
USART2->TDR
     │
     ▼
USART2 TX
     │
     ▼
PA2
```

---

# USART2 Configuration

Enable the GPIOA and USART2 clocks:

```c
RCC->AHB2ENR  |= (1U << 0);   /* GPIOA */
RCC->APB1ENR1 |= (1U << 17);  /* USART2 */
```

Configure PA2 and PA3 as Alternate Function:

```c
GPIOA->MODER &= ~(3U << (2U * 2U));
GPIOA->MODER |=  (2U << (2U * 2U));

GPIOA->MODER &= ~(3U << (3U * 2U));
GPIOA->MODER |=  (2U << (3U * 2U));
```

Select **AF7 = USART2**:

```c
GPIOA->AFR[0] &= ~(0xFU << (2U * 4U));
GPIOA->AFR[0] |=  (0x07U << (2U * 4U));

GPIOA->AFR[0] &= ~(0xFU << (3U * 4U));
GPIOA->AFR[0] |=  (0x07U << (3U * 4U));
```

---

# Baud Rate & USART Configuration

With the project running from the **16 MHz HSI clock**, the USART baud-rate register is configured as:

```c
USART2->BRR = 139U;
```

The USART is configured with:

```c
USART2->CR1 &= ~(0xFU << 0);
USART2->CR1 |=  (0xDU << 0);
```

This enables:

```text
UE  → USART enabled
TE  → Transmitter enabled
RE  → Receiver enabled
```

---

# Transmission

Character transmission uses the **TXE flag** to determine when the transmit data register can accept a new byte:

```c
void UART2_SendChar(char c)
{
    while (!(USART2->ISR & (1U << 7)))
    {
    }

    USART2->TDR = c;
}
```

Strings are transmitted character by character:

```c
void UART2_SendString(char *str)
{
    while (*str)
    {
        UART2_SendChar(*str++);
    }
}
```

Application example:

```c
UART2_SendString("Bare-Metal STM32G4 Online!\r\n");
```

---

# Application

The current implementation sends a status message periodically:

```text
Bare-Metal STM32G4 Online!
```

The delay is generated using the project's SysTick timebase:

```c
my_delay_ms(1000);
```

The project therefore combines:

```text
SysTick
   │
   └── Software timebase

TIM2
   │
   └── Hardware PWM

USART2
   │
   └── Serial communication
```

---

# Verification

Connect a USB-to-UART adapter or compatible serial interface to the USART2 pins.

Expected configuration:

```text
Baud rate : ~115200
Data      : 8 bits
Stop      : 1
Parity    : None
```

Expected terminal output:

```text
Bare-Metal STM32G4 Online!
```

Useful registers to inspect:

```text
RCC->APB1ENR1
GPIOA->MODER
GPIOA->AFR[0]

USART2->BRR
USART2->CR1
USART2->ISR
USART2->TDR
```

---

# Key Registers

| Register        | Function                   |
| --------------- | -------------------------- |
| `RCC->APB1ENR1` | USART2 clock enable        |
| `GPIOA->MODER`  | PA2/PA3 Alternate Function |
| `GPIOA->AFR[0]` | USART2 AF7 selection       |
| `USART2->BRR`   | Baud-rate configuration    |
| `USART2->CR1`   | USART enable / TX / RX     |
| `USART2->ISR`   | USART status flags         |
| `USART2->TDR`   | Transmit data register     |

---

# Engineering Notes

### Current architecture: polling

The transmitter currently waits for `TXE`:

```text
Application
    │
    ▼
Check TXE
    │
    ├── 0 → wait
    │
    └── 1 → write TDR
```

This is simple and useful for understanding the USART peripheral, but it is **blocking**.

### Future improvement

A future UART module can replace polling with:

```text
USART Interrupt
      │
      ▼
Ring Buffer
      │
      ▼
Non-blocking TX
```

This will allow the CPU to perform other tasks while UART transmission is handled asynchronously.

---

# Module Outcome

The project now includes a third hardware-driven peripheral:

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
```

The firmware continues to move toward a **deterministic, register-level and non-blocking embedded architecture**.

---

# Next Module

## Module 06 — Interrupt-Driven UART & Ring Buffer

Topics:

* USART interrupts
* RX/TX event handling
* Circular buffers
* Non-blocking communication
* Producer/consumer architecture
* UART command interface
* Interaction with SysTick and EXTI
