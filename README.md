# STM32G474RE Bare-Metal Firmware Engineering

A hands-on engineering journey into **deterministic, bare-metal embedded software development** on the **STM32G474RE**, based on the Arm® Cortex®-M4 architecture with FPU and DSP extensions, running at up to **170 MHz**.

The project focuses on developing embedded firmware with a strong emphasis on:

* **Direct hardware register manipulation using CMSIS**
* Deterministic and predictable execution
* Interrupt-driven peripheral architectures
* Non-blocking and asynchronous software design
* DMA-based data transfers
* Precise timing and synchronization
* Real-time control concepts
* Robust firmware architecture for safety- and performance-critical applications

The long-term objective is to build a solid firmware foundation applicable to **industrial automation, power electronics, motor control, instrumentation, and medical technology (Medizintechnik)**.

---

## Target Hardware

| Parameter                 | Specification                                   |
| ------------------------- | ----------------------------------------------- |
| **Microcontroller**       | STM32G474RET6                                   |
| **CPU**                   | Arm® Cortex®-M4 with FPU and DSP extensions     |
| **Maximum CPU Frequency** | 170 MHz                                         |
| **Evaluation Board**      | NUCLEO-G474RE                                   |
| **Timer / PWM**           | HRTIM, advanced-control timers                  |
| **Analog**                | High-speed ADCs, DACs, comparators, OPAMPs      |
| **Digital Interfaces**    | FDCAN, USART/UART, SPI, I²C                     |
| **Acceleration**          | CORDIC, FMAC                                    |
| **Data Transfer**         | DMA                                             |
| **Debug / Programming**   | ST-LINK / ST-LINK-V3                            |
| **Toolchain**             | STM32CubeIDE / GNU Arm Embedded Toolchain (GCC) |

> **Note:** Peripheral capabilities and exact resource allocation depend on the STM32G474 device variant and the specific board configuration.

---

# Engineering Roadmap

The project is developed incrementally, with each module introducing a specific embedded-software concept.

* [x] **Module 01 — GPIO & Direct Register Manipulation**
* [ ] **Module 02 — External Interrupts & NVIC Event Handling (EXTI)**
* [ ] **Module 03 — Precise Deterministic Timing with ARM Cortex-M SysTick**
* [ ] **Module 04 — Non-Blocking Asynchronous UART with Circular Ring Buffers**
* [ ] **Module 05 — Direct Memory Access (DMA) & CPU-Efficient Data Transfers**
* [ ] **Module 06 — Analog Subsystem & Mixed-Signal Control (ADC, DAC, Comparators)**
* [ ] **Module 07 — High-Resolution Timer (HRTIM) & Synchronous PWM Generation**
* [ ] **Module 08 — Real-Time Operating Systems (FreeRTOS / Zephyr Integration)**

---

# Repository Structure

```text
stm32g4-baremetal-journey/
│
├── README.md
│
├── docs/
│   ├── hardware/
│   ├── timing/
│   ├── diagrams/
│   └── notes/
│
├── 01_GPIO_Register_Blink/
│   ├── README.md
│   ├── Blinky_G474.ioc
│   │
│   └── Core/
│       ├── Inc/
│       │   └── ...
│       │
│       └── Src/
│           ├── main.c
│           └── system_stm32g4xx.c
│
├── 02_EXTI_Interrupts/
│   ├── README.md
│   └── Core/
│       ├── Inc/
│       └── Src/
│
└── ...
```

---

# Module 01 — GPIO & Direct Register Manipulation

## Objective

The first module establishes the fundamental principles of **bare-metal STM32 firmware development** by configuring and controlling GPIO peripherals directly through the MCU's memory-mapped registers.

The implementation deliberately avoids high-level blocking GPIO APIs and instead uses **CMSIS device headers and direct register access**.

The objective is not simply to blink an LED, but to understand exactly how the Cortex-M4 interacts with the STM32 peripheral subsystem.

---

## Core Concepts

### 1. Peripheral Clock Gating

Before accessing a GPIO peripheral, its clock must be enabled through the **Reset and Clock Control (RCC)** peripheral.

For example:

```c
RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
RCC->AHB2ENR |= RCC_AHB2ENR_GPIOCEN;
```

This enables the AHB2 clock for GPIOA and GPIOC.

Clock gating is an important STM32 architectural concept because unused peripheral clocks can remain disabled to reduce power consumption.

---

### 2. GPIO Mode Configuration

Each GPIO pin has a two-bit field in the `MODER` register.

For a pin `n`, the corresponding field is:

```text
MODER[2n+1 : 2n]
```

For example, configuring PA5 as a general-purpose output requires clearing its two-bit mode field and selecting output mode:

```c
GPIOA->MODER &= ~(3U << (5U * 2U));
GPIOA->MODER |=  (1U << (5U * 2U));
```

This demonstrates the standard embedded technique of:

1. Masking the existing bit field
2. Preserving unrelated configuration bits
3. Writing only the required field

---

### 3. Atomic GPIO Output Control with BSRR

Instead of performing a read-modify-write operation on `ODR`, the firmware uses the GPIO `BSRR` register.

Set PA5:

```c
GPIOA->BSRR = (1U << 5U);
```

Reset PA5:

```c
GPIOA->BSRR = (1U << (5U + 16U));
```

The `BSRR` mechanism provides an atomic set/reset operation without requiring a separate read of `ODR`.

This is preferable in situations where GPIO state can potentially be modified by multiple execution contexts, such as:

* Interrupt service routines
* Main-loop code
* Concurrent peripheral events

It also avoids unnecessary read-modify-write sequences.

---

### 4. Digital Input Evaluation

The push-button state is read directly from the GPIO input data register:

```c
uint32_t button_state = GPIOC->IDR & (1U << 13U);
```

The bit mask isolates the state of **PC13**, allowing the application to evaluate the button without affecting or reading unrelated GPIO inputs.

---

# Hardware Verification

The first module has been verified on the **NUCLEO-G474RE** evaluation board using:

* **PA5** — User LED
* **PC13** — User push-button

The firmware demonstrates:

```text
                    STM32G474RE
                 ┌───────────────┐
                 │               │
        PC13 ───►│ GPIOC / IDR   │
        Button   │               │
                 │               │
                 │               │
        PA5  ◄───│ GPIOA / BSRR  │
        LED      │               │
                 └───────────────┘
```

The hardware behavior was verified directly on the target board.

---

# Why Bare-Metal?

The purpose of this project is **not** to claim that HAL-based development is inherently incorrect.

STM32 HAL provides substantial benefits for:

* Rapid prototyping
* Portability
* Complex peripheral initialization
* Large application development
* Reducing development time

However, understanding the underlying hardware is essential when developing systems where **latency, determinism, timing, resource utilization, and hardware behavior** matter.

This project therefore intentionally moves down the abstraction stack:

```text
High-Level Application
        │
        ▼
Middleware / RTOS
        │
        ▼
HAL / LL
        │
        ▼
CMSIS Device Headers
        │
        ▼
Memory-Mapped Registers
        │
        ▼
STM32G4 Hardware
```

The goal is to understand every relevant layer rather than treating the MCU as a black box.

---

# Engineering Principles

Throughout the project, the following principles are emphasized:

### Determinism

Execution time and hardware response should be predictable wherever practical.

### Non-Blocking Design

Long blocking delays and polling loops are progressively replaced by:

* Interrupts
* Hardware timers
* DMA
* Event-driven state machines

### Hardware-Aware Software

Firmware behavior is designed with explicit knowledge of:

* Clock domains
* Peripheral registers
* Interrupt priorities
* Bus architecture
* DMA transfers
* Timer synchronization
* ADC triggering
* Memory access patterns

### Separation of Concerns

Hardware initialization, peripheral drivers, application logic, and timing mechanisms should remain clearly separated.

### Measurable Behavior

Where possible, timing claims should be validated using measurement equipment such as:

* Oscilloscope
* Logic analyzer
* SWV / ITM
* Debugger trace capabilities

Rather than relying exclusively on source-code inspection.

---

# Future Development

The next stages progressively introduce more realistic embedded-system architectures.

## Module 02 — EXTI & NVIC

Transition from polling-based input handling to **hardware-triggered interrupt processing**.

Topics:

* EXTI configuration
* NVIC configuration
* Interrupt service routines
* Interrupt latency
* Event flags
* ISR design rules

---

## Module 03 — SysTick & Deterministic Timing

Develop a system time base without relying on blocking delay functions.

Topics:

* Cortex-M SysTick
* Tick counters
* Time comparisons
* Non-blocking delays
* Periodic tasks
* Timing jitter

---

## Module 04 — Asynchronous UART

Implement a non-blocking UART architecture based on:

* Interrupt-driven RX/TX
* Circular ring buffers
* Producer/consumer principles
* Framing
* Error handling

---

## Module 05 — DMA

Move repetitive data transfers from the CPU to dedicated DMA hardware.

Topics:

* DMA channel configuration
* Peripheral-to-memory transfers
* Memory-to-peripheral transfers
* Circular mode
* Transfer-complete interrupts
* Double-buffering concepts

---

## Module 06 — Analog Subsystem

Explore the STM32G4 mixed-signal capabilities.

Topics:

* ADC configuration
* Hardware triggering
* DMA-based sampling
* DAC
* Analog comparators
* Operational amplifiers
* Sampling timing
* Digital filtering

---

## Module 07 — HRTIM & Synchronous PWM

Develop deterministic PWM generation for applications such as:

* Power converters
* Motor control
* Digital power
* Synchronous rectification

Topics:

* HRTIM architecture
* Timer synchronization
* Dead-time generation
* ADC triggering
* Fault inputs
* High-resolution timing

---

## Module 08 — RTOS Integration

After establishing a strong bare-metal foundation, the project will investigate RTOS-based architectures using:

* **FreeRTOS**
* **Zephyr**

The goal is to understand what an RTOS adds on top of the hardware-level concepts developed in the previous modules.

---

# Engineering Perspective

This repository is intended to document the progression from **individual register-level operations** toward complete, event-driven embedded architectures.

The development path can be summarized as:

```text
GPIO
  │
  ▼
Interrupts
  │
  ▼
Deterministic Timing
  │
  ▼
Asynchronous Communication
  │
  ▼
DMA
  │
  ▼
ADC / DAC / Analog Control
  │
  ▼
HRTIM / PWM / Real-Time Control
  │
  ▼
RTOS-Based System Architecture
```

The final objective is to build firmware that is not only functional, but also **predictable, measurable, maintainable, and conscious of the underlying hardware architecture**.

---

## Author

**Baurel Kaptouom**
Master's degree in Electrical and Information Engineering
Focus: Embedded Systems · Firmware · Real-Time Systems · Electronics

---

## License

This repository is intended primarily as an engineering learning and portfolio project.

License information will be added as the project evolves.
