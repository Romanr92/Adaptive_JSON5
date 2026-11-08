# Memory layout reference

A quick reference for where code and data live in memory, per environment: the
sections (`.text`, `.rodata`, `.data`, `.bss`), the heap and the stack, and in which
direction each one grows. It is general background for developers, not a requirement
of the library. Adaptive_JSON5's parser library itself uses no heap: only the
caller's buffer and the stack.

**Caveat: the CPU decides very little of this.** The linker script, the loader and the
operating system decide where each section goes. The pictures show a typical layout,
not a guarantee; check the map file of your own build. In every picture, high
addresses are at the top, and small arrows show the direction in which a region grows.

## The sections

| Section | Contains | Where it lives |
| --- | --- | --- |
| Vector table | Reset vector and interrupt handler addresses (microcontrollers) | Flash, usually at the start |
| `.text` | Machine code | Flash, or read-only and executable pages |
| `.rodata` | Constants, string literals, lookup tables | Flash, or read-only pages |
| `.data` | Global and static variables with an initial value | RAM. On a microcontroller the initial values are stored in flash and **copied to RAM by the startup code** |
| `.bss` | Global and static variables that start as zero | RAM, **cleared to zero by the startup code** (or by the OS); takes no space in the file |
| Heap | Memory handed out at run time (`malloc`, `new`) | RAM, grows **up**. Often absent on safety-related microcontrollers |
| Stack | Return addresses, saved registers and local variables | RAM, grows **down** on all architectures covered here |

---

## 1. Bare metal and RTOS

On a microcontroller everything is decided at build time by the linker script: there
is no loader and usually no virtual memory. Code and constants stay in flash; `.data`,
`.bss` and the stacks are in RAM.

### 1.1 AUTOSAR Classic OS (example: Vector MICROSAR)

> **Licensed product.** Vector MICROSAR is a commercial product, and its documentation
> is licensed. The picture below is a **typical example of an AUTOSAR Classic memory
> structure**, drawn from the public AUTOSAR concepts. It is not Vector's actual layout,
> and nothing in it is copied from Vector's documentation. Your project's configuration
> and linker script decide the real layout.

![AUTOSAR Classic OS, typical: flash holds the vector table, .text, .rodata, the .data load image and calibration data; RAM holds OS and task stacks, per-partition data and shared data, with no heap.](images/memory-layout-microsar.svg)

- **Everything is static.** The configuration tool generates the memory mapping, and
  the linker script places every section. A typical project has **no heap**.
- **Every task and interrupt group has its own fixed-size stack**, plus kernel stacks
  per core. The OS can monitor them for overflow (stack monitoring), and with memory
  protection the MPU guards them.
- **OS-Applications are separated into memory partitions.** With memory protection
  enabled, each partition's data sits in its own MPU region, so a fault in one cannot
  overwrite another.

### 1.2 FreeRTOS

![FreeRTOS on a Cortex-M: flash holds the vector table, .text, .rodata and the .data load image; RAM holds the main stack, the FreeRTOS heap array, static task stacks and the globals.](images/memory-layout-freertos.svg)

- **Every task has its own stack.** With `xTaskCreate()`, the stack and the task control
  block come from the FreeRTOS heap: an array `ucHeap[configTOTAL_HEAP_SIZE]` in `.bss`,
  managed by one of the `heap_1` to `heap_5` schemes. With `xTaskCreateStatic()`, the
  application provides the stack as its own array.
- The **main stack** is used by the startup code and, on Cortex-M, by interrupts.
- `configCHECK_FOR_STACK_OVERFLOW` adds a software check at context switches; an MPU
  port can add a hardware guard below each task stack.

### Keep in mind: architecture differences (microcontrollers)

| Topic | ARM Cortex-M | RISC-V |
| --- | --- | --- |
| Stack direction | Down, **enforced by hardware**: `PUSH` and exception entry always lower `sp` | Down **by convention only** (psABI); there is no push or pop instruction |
| Initial stack pointer | First word of the vector table | Set by the startup code |
| Stack alignment | 8 bytes at function calls (AAPCS); the hardware aligns exception frames | 16 bytes (psABI) |
| Interrupts and the stack | The hardware pushes an exception frame onto the active stack | Software saves registers; no automatic frame |
| Two stack pointers | `MSP` (main, interrupts) and `PSP` (tasks) | One `sp`; RTOS switches it per task |
| Overflow detection in hardware | ARMv8-M: `MSPLIM` and `PSPLIM` stack limit registers. ARMv7-M (for example Cortex-M4): an MPU guard region only | No stack limit register in the base ISA: a PMP guard region, if configured |

---

## 2. Linux

![Linux process: kernel space at the top, the stack growing down, the mmap area, the heap growing up, then .bss, .data, .rodata, .text and an unmapped page 0.](images/memory-layout-linux.svg)

- Every process has its own virtual address space. The executable is an **ELF** file.
- The **stack** of the main thread is at the top of user space and grows down, with a
  guard gap below it. `argv`, the environment and the auxiliary vector sit at its top.
- The **mmap area** holds shared libraries, thread stacks (each with a guard page) and
  large allocations; new mappings are placed top-down by default.
- The **heap** (`brk`) starts after `.bss` and grows up; it holds small allocations.
- **Page 0 is unmapped**, so dereferencing a null pointer faults.
- **ASLR** randomizes the base of the stack, the mmap area, the heap and
  position-independent executables on every start.

### Keep in mind: architecture differences (Linux)

| Topic | x86-64 | AArch64 | 32-bit ARM | RISC-V 64 | LoongArch64 |
| --- | --- | --- | --- | --- | --- |
| Stack direction | Down, enforced by hardware | Down (by convention) | Down (full-descending, required by the AAPCS) | Down (by convention) | Down (by convention) |
| Stack alignment | 16 bytes at calls | 16 bytes, can be checked by hardware | 8 bytes at calls | 16 bytes | 16 bytes |
| Red zone | **128 bytes** below `rsp` that functions may use without moving it | None | None | None | None |
| Page size | 4 KB | 4, 16 or 64 KB, depending on the kernel | 4 KB | 4 KB | Often 16 KB |
| User address space | 47 bits (128 TB) usually | 39 or 48 bits, depending on the kernel | About 3 GB | Depends on the paging mode (Sv39, Sv48) | Depends on the kernel |

The **red zone** matters for low-level code: signal handlers and the kernel skip those
128 bytes, but code that runs on someone else's stack, such as kernel code or interrupt
handlers, must be compiled without it (`-mno-red-zone`).

---

## 3. Windows

![Windows process: kernel space, DLLs, a thread stack with a guard page and reserved space below, process heaps and the EXE image.](images/memory-layout-windows.svg)

- The executable format is **PE**, not ELF. Its sections are usually `.text`, `.rdata`
  (read-only data) and `.data`; zero-initialized data is normally part of `.data`.
- **Each thread reserves its stack up front** (1 MB by default) but commits only the
  pages it uses. A **guard page** sits below the committed part; touching it commits the
  next page and moves the guard down. When the reserve is used up, the thread gets a
  stack overflow exception.
- A process can have **several heaps** (the process heap, the C runtime heap and
  others), so there is no single heap region.
- The loader and the memory manager place the image, DLLs, heaps and stacks, and
  **ASLR** randomizes them. The order in the picture is only one possibility.

### Keep in mind: architecture differences (Windows)

| Topic | x64 | ARM64 | x86 (32-bit) |
| --- | --- | --- | --- |
| Stack direction | Down, enforced by hardware | Down (by convention) | Down, enforced by hardware |
| Stack alignment | 16 bytes at calls | 16 bytes | 4 bytes traditionally |
| Red zone | **None**: unlike x86-64 Linux, nothing below `rsp` may be used | None | None |
| Shadow space | The caller reserves **32 bytes** above the return address for the callee | None | None |
| Large stack frames | Frames bigger than a page are touched page by page (`__chkstk`) so they cannot skip the guard page | Same | Same |
| Page size | 4 KB | 4 KB | 4 KB |

---

## 4. FreeBSD

![FreeBSD process: kernel space, the shared page, the stack growing down, the mmap area, the heap growing up, then data and code.](images/memory-layout-freebsd.svg)

- Executables are **ELF**, as on Linux, and the order of the regions is similar.
- The **shared page** near the top of user space holds the signal trampoline and
  timekeeping data.
- The **stack** of the main thread grows down from below it, with a guard gap.
- The default allocator (jemalloc) gets its memory mostly through `mmap`.
- **ASLR is enabled by default for 64-bit programs since FreeBSD 13.2.** With ASLR, the
  stack and the shared page can also be randomized.

### Keep in mind: architecture differences (FreeBSD)

| Topic | amd64 | arm64 | riscv64 |
| --- | --- | --- | --- |
| Stack direction | Down, enforced by hardware | Down (by convention) | Down (by convention) |
| Stack alignment | 16 bytes at calls | 16 bytes | 16 bytes |
| Red zone | **128 bytes**, as on x86-64 Linux | None | None |
| Page size | 4 KB | 4 KB | 4 KB |

---

## Where this is used

The [cybersecurity assumptions](ISO21434-cybersecurity/cybersecurity-assumptions.md#ca-03-how-memory-is-laid-out-and-why-it-matters)
explain, per architecture, why the stack layout matters for an attacker and where an
integrator should place guard regions.
