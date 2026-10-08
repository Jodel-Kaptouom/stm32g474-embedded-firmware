# Module 05: Bare-Metal UART Communication using USART2

This module implements a **register-level USART2 driver** on the STM32G474RE, without the STM32 HAL UART driver.

USART2 is routed through the **ST-LINK V3 Virtual COM Port (VCP)**, so the board talks to a serial terminal on the host PC over the same USB cable used for flashing and debugging.

---

## Table of Contents

* [Objectives](#objectives)
* [Hardware Configuration](#hardware-configuration)
* [USART2 Configuration](#usart2-configuration)
* [Baud Rate & USART Configuration](#baud-rate--usart-configuration)
* [Transmission](#transmission)
* [Reception](#reception)
* [printf Retargeting](#printf-retargeting)
* [Application](#application)
* [Verification](#verification)
* [Key Registers](#key-registers)
* [Engineering Notes](#engineering-notes)
* [Module Outcome](#module-outcome)
* [Next Module](#next-module)

---

# Objectives

* Configure USART2 using direct register access.
* Configure PA2/PA3 for UART communication.
* Set the baud rate to **115200 baud** from the 16 MHz HSI clock.
* Implement character and string transmission.
* Implement character reception.
* Retarget `printf` to USART2.
* Control the TIM2 PWM duty cycle from a serial terminal.
* Understand the TX/RX polling mechanism.
* Prepare the architecture for interrupt-driven UART and ring buffers.

---

# Hardware Configuration

| Item            | Value                                        |
| --------------- | -------------------------------------------- |
| Microcontroller | STM32G474RE (ARM Cortex-M4)                  |
| Peripheral      | USART2 on APB1                               |
| Clock           | 16 MHz HSI                                   |
| USART2_TX       | `PA2`, Alternate Function 7                  |
| USART2_RX       | `PA3`, Alternate Function 7                  |
| Frame format    | 115200 baud, 8 data bits, no parity, 1 stop  |
| Host interface  | ST-LINK V3 Virtual COM Port over USB         |

Signal routing:

```text
[ STM32G474RE ]                  [ ST-LINK V3 ]               [ Host PC ]

 PA2 (TX, AF7) ───── UART ─────▶ VCP RX ─┐
                                         ├── USB CDC ──────▶ Serial terminal
 PA3 (RX, AF7) ◀──── UART ────── VCP TX ─┘                   (115200 8N1)
```

Software layers:

```text
Application / printf()
        │
        ▼
UART2_SendString()  /  _write()
        │
        ▼
UART2_SendChar()
        │
        ▼
USART2->TDR
        │
        ▼
PA2 (TX)
```

---

# USART2 Configuration

Enable the GPIOA and USART2 clocks:

```c
RCC->AHB2ENR  |= (1U << 0);   /* GPIOA */
RCC->APB1ENR1 |= (1U << 17);  /* USART2 */
```

Configure PA2 and PA3 as Alternate Function (`MODER = 10b`):

```c
GPIOA->MODER &= ~((3U << (2U * 2U)) | (3U << (3U * 2U)));
GPIOA->MODER |=  ((2U << (2U * 2U)) | (2U << (3U * 2U)));
```

Select **AF7 = USART2** in `AFR[0]` (AFRL):

```c
GPIOA->AFR[0] &= ~((0xFU << (2U * 4U)) | (0xFU << (3U * 4U)));
GPIOA->AFR[0] |=  ((7U   << (2U * 4U)) | (7U   << (3U * 4U)));
```

---

# Baud Rate & USART Configuration

With oversampling by 16 (`OVER8 = 0`), the baud-rate divider is the peripheral clock divided by the target baud rate:

```text
USARTDIV    = f_CK / baud = 16 000 000 / 115 200 = 138.89
BRR         = 139 (0x8B)

Actual baud = 16 000 000 / 139 = 115 108 baud
Error       = (115 108 - 115 200) / 115 200 = -0.08 %
```

The error is far below the roughly 2 % a UART link tolerates.

```c
USART2->BRR = 139U;           /* 115200 baud @ 16 MHz */

USART2->CR1 = (1U << 3)       /* TE: transmitter enable */
            | (1U << 2)       /* RE: receiver enable    */
            | (1U << 0);      /* UE: USART enable       */
```

`BRR` is written before `UE` is set, because the baud rate must not be changed while the USART is enabled.

---

# Transmission

Character transmission polls the **TXE flag** (`ISR` bit 7), which is set when the transmit data register can accept a new byte:

```c
void UART2_SendChar(char c)
{
    while (!(USART2->ISR & (1U << 7)))   /* wait for TXE = 1 */
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

Example:

```c
UART2_SendString("Bare-Metal STM32G4 Online!\r\n");
```

---

# Reception

Character reception polls the **RXNE flag** (`ISR` bit 5), which is set when a received byte is waiting in the receive data register:

```c
char USART2_GetChar(void)
{
    while (!(USART2->ISR & (1U << 5)))   /* wait for RXNE = 1 */
    {
    }

    return (char)(USART2->RDR & 0xFFU);  /* reading RDR clears RXNE */
}
```

---

# printf Retargeting

The C library sends `stdout` through the low-level `_write()` syscall. Overriding it routes `printf` to USART2:

```c
int _write(int file, char *ptr, int len)
{
    for (int i = 0; i < len; i++)
    {
        UART2_SendChar(*ptr++);
    }

    return len;
}
```

Example:

```c
#include <stdio.h>

printf("Duty cycle: %d\r\n", 50);
```

`stdout` is buffered by the C library, so text is sent when a newline is printed or after `fflush(stdout)`. Buffering can be disabled once at startup with `setvbuf(stdout, NULL, _IONBF, 0)`.

---

# Application

The application uses the driver in both directions.

**TX:** the firmware sends a status message:

```text
Bare-Metal STM32G4 Online!
```

**RX:** single-character commands from the terminal set the TIM2 PWM duty cycle from Module 04:

| Key | Action             | Duty cycle |
| --- | ------------------ | ---------- |
| `1` | `TIM2->CCR1 = 999` | 100 %      |
| `5` | `TIM2->CCR1 = 500` | 50 %       |
| `0` | `TIM2->CCR1 = 0`   | 0 %        |

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
   └── Serial communication and PWM control
```

---

# Verification

No external USB-to-UART adapter is needed: the ST-LINK Virtual COM Port carries USART2 over the debug USB cable.

1. Connect the board over USB.
2. Find the port number in the Windows Device Manager, under *Ports (COM & LPT)*: `STMicroelectronics STLink Virtual COM Port (COMx)`.
3. Open a serial terminal on that port: the STM32CubeIDE *Command Shell Console* (Serial Port), PuTTY or Tera Term.

Terminal settings:

```text
Baud rate : 115200
Data      : 8 bits
Stop      : 1
Parity    : None
```

Expected output:

```text
Bare-Metal STM32G4 Online!
```

Then press `1`, `5` and `0` and check that the PWM duty cycle follows.

Troubleshooting:

| Symptom                      | Likely cause                                                   |
| ---------------------------- | -------------------------------------------------------------- |
| `Error opening \\.\COMx (5)` | The port is already open in another terminal or console        |
| Unreadable characters        | Baud rate mismatch between `BRR` and the terminal              |
| No output                    | Wrong COM port, USART2 clock not enabled, or PA2 not set to AF7 |

Useful registers to inspect in the debugger:

```text
RCC->APB1ENR1
GPIOA->MODER
GPIOA->AFR[0]

USART2->BRR
USART2->CR1
USART2->ISR
USART2->TDR
USART2->RDR
```

---

# Key Registers

| Register        | Function                           |
| --------------- | ---------------------------------- |
| `RCC->AHB2ENR`  | GPIOA clock enable                 |
| `RCC->APB1ENR1` | USART2 clock enable                |
| `GPIOA->MODER`  | PA2/PA3 Alternate Function mode    |
| `GPIOA->AFR[0]` | USART2 AF7 selection               |
| `USART2->BRR`   | Baud-rate configuration            |
| `USART2->CR1`   | USART enable / TX / RX             |
| `USART2->ISR`   | Status flags (TXE bit 7, RXNE bit 5) |
| `USART2->TDR`   | Transmit data register             |
| `USART2->RDR`   | Receive data register              |

---

# Engineering Notes

### Current architecture: polling

Both directions wait on a status flag:

```text
TX                              RX

Check TXE                       Check RXNE
    │                               │
    ├── 0 → wait                    ├── 0 → wait
    │                               │
    └── 1 → write TDR               └── 1 → read RDR
```

This is simple and useful for understanding the USART peripheral, but it is **blocking**. The receive side is the more limiting one: `USART2_GetChar()` does not return until a key is pressed, so the CPU can do nothing else in the meantime.

### Future improvement

A future UART module can replace polling with:

```text
USART Interrupt
      │
      ▼
Ring Buffer
      │
      ▼
Non-blocking TX / RX
```

This will allow the CPU to perform other tasks while UART communication is handled asynchronously.

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