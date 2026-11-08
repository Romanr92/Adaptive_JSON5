# Cybersecurity assumptions

| Item | Value |
| --- | --- |
| Work product | Assumptions for developing the library as a cybersecurity component out of context (ISO/SAE 21434) |
| Issue | [#139](https://github.com/Romanr92/Adaptive_JSON5/issues/139) |
| Status | Draft, not reviewed. The owner discussion is still in progress. |
| Scope | Parser library `AdaptiveJSON5::parser`. The hosted add-on and the planned serializer are not covered yet. |

The library is developed without knowing the vehicle, the ECU or the network it will
be used in, in the same way as for functional safety (see the
[SEooC assumptions](../ISO26262-safety/seooc-assumptions.md)). The full threat
analysis and risk assessment (TARA) of the item, with damage scenarios and impact
ratings, is done by the integrator at vehicle level. This document records what the
library assumes instead. The integrator must confirm each assumption against their
own item and their own TARA.

No compliance, certification or "secure" status is claimed in this document.

## How a CAL is determined (general method)

This section explains how a CAL is normally worked out, so that the reasoning in the
next section can be followed. It is a summary in our own words. The standard's text is
licensed: for a real rating, use ISO/SAE 21434 itself.

A CAL (Cybersecurity Assurance Level) says **how much rigour** the cybersecurity work
needs: how deep the analysis, the testing and the reviews go, and how independent the
reviewers are. It comes from an **informative** annex of ISO/SAE 21434 (Annex E), so it
is guidance rather than a mandatory formula. It is usually set early, in the concept
phase, for each cybersecurity goal.

Unlike the ASIL, which combines three factors, a CAL combines **two**: the impact of the
damage and the attack vector.

### Legend: the two factors

**Impact: how bad the damage would be.** ISO/SAE 21434 rates it in four categories
(safety, financial, operational and privacy) and uses the worst one.

| Impact | Meaning | Example from the safety category |
| --- | --- | --- |
| Negligible | No noticeable damage | No injuries (like S0) |
| Moderate | Noticeable but limited damage | Light or moderate injuries (like S1) |
| Major | Serious damage | Severe injuries, survival probable (like S2) |
| Severe | Very serious damage | Life-threatening or fatal injuries (like S3) |

**Attack vector: how the attacker gets in.** The more remote the attack, the higher the
rating, because more attackers can reach it.

| Attack vector | Meaning | Example |
| --- | --- | --- |
| Physical | The attacker must touch the hardware | Opening the ECU, probing the circuit board |
| Local | The attacker needs local access to the vehicle or ECU | A USB stick, an SD card, the diagnostic port |
| Adjacent | The attacker must be nearby or on a local network | Bluetooth, Wi-Fi, short-range radio, the same in-vehicle bus |
| Network | The attacker can be anywhere | Cellular, internet, remote services, messages relayed over Ethernet |

### The CAL table

| Impact | Physical | Local | Adjacent | Network |
| --- | --- | --- | --- | --- |
| Severe | CAL 2 | CAL 3 | CAL 4 | **CAL 4** |
| Major | CAL 1 | CAL 2 | CAL 3 | CAL 4 |
| Moderate | CAL 1 | CAL 1 | CAL 2 | CAL 3 |
| Negligible | — | — | — | — |

### The levels and their rigour

| Level | Assurance | Rigour |
| --- | --- | --- |
| No CAL | — | **No extra rigour** for this goal. Only the basic organisation-wide cybersecurity work applies. |
| CAL 1 | Low to moderate | **Basic** |
| CAL 2 | Moderate | **Moderate** |
| CAL 3 | Moderate to high | **Thorough** |
| CAL 4 | High | **Most thorough** |

**Is there something like QM?** Not by name. The closest equivalent is "no CAL", which
applies when the impact is negligible. Unlike QM in safety, nothing is ever fully outside
ISO/SAE 21434: basic work such as vulnerability monitoring and handling applies to every
product.

ISO/SAE 21434 does not list required methods per CAL the way ISO 26262 does per ASIL. It
describes how rigour and independence should grow with the level. The sections below say
what each rigour level means in practice. They are our interpretation, written for this
kind of library; the suggested tools are examples, not requirements of the standard.

#### Basic (CAL 1)

What has to be done:

- **Analysis:** a TARA of the component and the cybersecurity requirements derived from it.
- **Development:** follow the coding guidelines; treat compiler warnings as errors.
- **Testing:** requirements-based tests of the security functions and of failure handling.
- **Reviews:** code and document reviews by **another person**, who may be on the same team.
- **Vulnerabilities:** a way to receive reports and to fix them.

Suggested tools and activities: compiler warnings as errors (`-Wall -Wextra -Werror`), unit
tests (for example GoogleTest), peer review in pull requests, `SECURITY.md`.

#### Moderate (CAL 2)

Everything from Basic, plus:

- **Analysis:** a more systematic look at weaknesses in the design and the interfaces.
- **Testing:** interface tests and tests with malformed or unexpected input.
- **Supply chain:** know every dependency and its version.
- **Reviews:** still by another person; findings are tracked until they are closed.

Suggested tools and activities: static analysis (for example clang-tidy or cppcheck), an
SBOM (for example SPDX or CycloneDX), pinned dependency versions.

#### Thorough (CAL 3)

Everything from Moderate, plus:

- **Analysis:** a deeper vulnerability analysis of every attack path from the TARA.
- **Testing:** fuzz testing, testing with memory error detection, and **penetration testing**.
- **Supply chain:** automatic scanning for known vulnerabilities in dependencies and CI.
- **Reviews:** by a person from **another team**, not under the same lead as the developer.

Suggested tools and activities: fuzzing (for example libFuzzer or AFL++), AddressSanitizer
and UndefinedBehaviorSanitizer, dependency scanning (for example Dependabot or
OSV-Scanner), security-focused static analysis (for example CodeQL), penetration testing.

#### Most thorough (CAL 4)

Everything from Thorough, plus:

- **Analysis:** the deepest analysis, covering every interface and every path through the
  security-relevant code.
- **Testing:** continuous fuzzing, and penetration testing by specialists.
- **Reviews and assessment:** by a person or organisation **independent of the development
  team**, ideally an external security assessor.
- **Evidence:** a complete cybersecurity case that ties every claim to its evidence.

Suggested tools and activities: continuous fuzzing (for example OSS-Fuzz or a scheduled
long fuzz run in CI), coding-standard checks (for example CodeQL with MISRA C++ queries),
an independent security audit.

### Risk treatment

After the TARA, every risk gets one of four treatments:

| Treatment | Meaning | Example |
| --- | --- | --- |
| **Avoid** | Remove the source of the risk, for example by dropping a feature | The library never interprets string content (T-06) |
| **Reduce** | Add protections that lower the risk | Limits, bounds checks, fuzzing (T-01, T-03) |
| **Share** | Hand part of the risk to another party, by agreement | The integrator authenticates input before parsing |
| **Retain** | Accept a low risk without further protections, with a recorded reason | A residual risk after all other treatments |

"Retain" is the closest idea to QM in safety: a deliberate, documented decision that no
further measures are needed.

### When an integrator may use a lower CAL

The library is developed towards CAL 4 because it does not know its use. An integrator's
own TARA can legitimately arrive at a lower CAL, for example when:

- the input only arrives through **local or physical** access, such as a configuration file
  installed in the factory or a signed software update that is verified before parsing;
- the **impact** of a successful attack would be only **moderate or major**, for example
  for a comfort or information-only function.

That is fine: a component developed for a higher CAL can be used where a lower one is
needed. The reverse is not true.

**Differences from the ASIL:**

- A CAL is **informative**; the ASIL is normative.
- There is **no decomposition** for a CAL. ISO/SAE 21434 has no rule for splitting a CAL
  between two parts, as ISO 26262 has for the ASIL.
- The CAL scheme is still evolving. ISO/SAE PAS 8475 is developing a revised scheme
  (drafts reduce it to three levels). This document uses the four levels of
  ISO/SAE 21434:2021.

## How we arrived at CAL 4

This is the reasoning behind the assumed CAL, step by step.

1. **There is no item TARA for a library.** As with the ASIL, a JSON parser has no damage
   scenarios of its own. As a component out of context it **assumes** a CAL, and each
   integrator's TARA must confirm it.
2. **We do not know the projects it will be used in.** So, as for safety, we assume the
   worst case.
3. **Impact: severe.** The library sits in a safety path: on the safety side it is
   assumed to support functions up to ASIL D (as one part of a decomposition, see
   [How we arrived at ASIL B(D)](../ISO26262-safety/seooc-assumptions.md#how-we-arrived-at-asil-bd)).
   A successful attack could therefore lead to life-threatening harm, which is the
   **severe** impact level.
4. **Attack vector: network.** The library parses data that can arrive over vehicle
   networks such as Ethernet, and the attacker is assumed to fully control that input
   (CA-01). That is the **network** attack vector.
5. **The table gives severe + network = CAL 4.**
6. **The CAL is not lowered by the integrator's protection.** For safety, the integrator's
   E2E check lets the library drop from ASIL D to B(D). For cybersecurity this does not
   work in the same way:
   - there is no decomposition for a CAL;
   - even if the integrator authenticates every message, the sender itself can be
     compromised, and then authentic messages carry crafted content. The library must
     still survive that input on its own.
7. **Result: the library assumes CAL 4**, the same "assume the worst" choice as for
   safety. This means the highest rigour: deep verification such as fuzzing, and
   reviewers independent of the developer. Until an independent reviewer is found, the
   same gap applies as for safety (see the [safety plan](../ISO26262-safety/safety-plan.md#1-roles)).

## CA-01 Attacker model

- **The attacker fully controls the JSON input.** Every byte, its length and its
  timing can be chosen by the attacker, for example by sending crafted messages over
  Ethernet or another vehicle network. This matches the safety assumption that all
  input is untrusted ([A-03](../ISO26262-safety/seooc-assumptions.md#a-03-all-input-is-untrusted)).
- **The attacker cannot change the library's code or memory directly.** If an attacker
  could already write to the ECU's memory or replace its software, the library could
  not defend itself. Preventing that is the integrator's job, for example with secure
  boot, signed software updates and memory protection.
- **The library's memory is bounded.** The library never uses the heap. All its storage
  comes from the buffer or memory functions the integrator provides, plus the call
  stack, and every resource is capped by the integrator's limits
  ([A-04](../ISO26262-safety/seooc-assumptions.md#a-04-the-integrator-sets-the-resource-limits),
  [A-05](../ISO26262-safety/seooc-assumptions.md#a-05-memory-no-heap-and-which-library-is-which)).
  A whole class of attacks therefore does not apply: there is no heap metadata to
  corrupt and no way to make the library take more memory than it was given.
- **Deep nesting cannot exhaust the stack through recursion.** The parser keeps open
  containers on an explicit stack in the caller's buffer instead of using recursion.
  The real stack use is still to be measured (#136).

## CA-02 What the attacker wants

Ordered from most to least serious:

| ID | Goal | Example at the library boundary |
| --- | --- | --- |
| G-1 | **Take control** | Use a memory bug such as a buffer overflow to write outside the library's buffer, corrupt other data, or run the attacker's own code. From there the attacker could reach other parts of the ECU. |
| G-2 | **Break integrity** | Make the library accept bad or tampered data as valid, or make two parsers read the same message differently. |
| G-3 | **Break availability** | Make the library hang, crash, run too long, or reject valid data. |
| G-4 | **Break confidentiality** | Make the library leak memory contents, for example through error messages or retained copies of the input. |

G-1 is the top threat. The defences line up with the safety work: the library never
writes outside the memory it was given
([SSR-08](../requirements.md#k-software-safety-requirements-iso-26262)), fuzzing and
sanitizers hunt for exactly these bugs
([verification strategy](../ISO26262-safety/verification-strategy.md)), and memory
protection on the integrator's side keeps an overflow from spreading
([DFA-04](../ISO26262-safety/software-safety-analysis.md#2-dependent-failure-analysis)).

## CA-03 How memory is laid out, and why it matters

A memory bug is only dangerous because of what lies next to the memory it touches.
This section explains, per architecture, where the stack and the heap are and in
which direction they grow, so that integrators can place guard regions correctly.
The pictures show normal operation only.

**Caveat: this depends on the architecture, the operating system and the linker
script.** The pictures show the usual layout, not a guarantee.

The classic picture: the stack starts at a high address and grows **down**, the heap
starts lower and grows **up**, so they grow toward each other. Adaptive_JSON5 itself
uses no heap: only the integrator's buffer and the stack.

### Bare metal (no operating system)

![Bare metal: the main stack at the top of RAM grows down; below it free RAM, a guard region, the Adaptive_JSON5 buffer, .bss and .data. No heap.](images/stack-heap-bare-metal.svg)

- This is the most important case for the library: a microcontroller with no
  operating system, or a small RTOS, and usually **no heap at all**.
- Startup code sets the stack pointer, typically to the top of RAM, and the stack
  grows down from there.
- Below the stack there is usually static data: global variables in `.bss` and
  `.data`, and often the Adaptive_JSON5 buffer as a static array.
- **Without memory protection, nothing stops the stack at its end.** It would silently
  continue into the global data below it. The integrator must place a guard region
  below the stack: an MPU region, a PMP region on RISC-V, or the stack limit registers
  on ARMv8-M.

### x86 and x64

![x86 and x64: kernel space at the top, the main thread stack growing down, mappings, the heap growing up, then data and code.](images/stack-heap-x86-x64.svg)

- The stack grows down, and **the hardware enforces this direction**: `PUSH`, `CALL`
  and interrupt entry always lower the stack pointer (`esp` or `rsp`).
- Under an operating system the heap sits lower and grows up. Every thread gets its
  own stack with a guard page below it.
- Address space layout randomization (ASLR) moves the stack, heap and libraries to a
  random base on every start, which makes memory bugs much harder to exploit.

### ARM

![ARM: on Cortex-M the main stack at the top of RAM grows down with a stack limit or MPU guard below; on Cortex-A under Linux the usual process layout.](images/stack-heap-arm.svg)

- The stack grows down. On **Cortex-M**, the hardware itself pushes registers onto the
  active stack on every interrupt, downwards, and the initial stack pointer comes from
  the first entry of the vector table, usually the top of RAM.
- **ARMv8-M cores** (for example Cortex-M33) have stack limit registers, `MSPLIM` and
  `PSPLIM`, that raise a fault when the stack goes past its limit.
- **Older cores** such as the Cortex-M4 have no stack limit registers and need an MPU
  guard region below each stack.
- **Cortex-A** under Linux uses the usual process layout; on AArch64 the stack pointer
  is kept 16-byte aligned.

### RISC-V

![RISC-V: the stack grows down by software convention, with a PMP guard region below it, then the Adaptive_JSON5 buffer and the globals.](images/stack-heap-riscv.svg)

> **⚠️ RISC-V gives the least built-in help.** This is the main target for the
> library, so read this one carefully.

- RISC-V has **no push or pop instructions**. The stack pointer is simply register
  `x2` (`sp`), and functions lower it themselves (`addi sp, sp, -N`).
- The direction is therefore **only a software convention**: the standard calling
  convention (psABI) has the stack grow down and keeps `sp` 16-byte aligned. Nothing
  in the hardware enforces that direction.
- **Nothing in the hardware catches an overflow either.** The base ISA has no stack
  limit register. Protection comes from **PMP (physical memory protection)**, set up by
  the firmware or OS as a guard region below each stack. If nobody configures it,
  there is no protection.
- **A PMP guard does not automatically apply to machine mode.** Under the standard PMP
  rules, an entry that is not **locked** (L bit clear) is ignored for ordinary accesses
  from M-mode, which is where bare-metal code usually runs. So either lock the guard
  entry, run the code in a lower privilege mode (U-mode or S-mode), or use the
  Smepmp extension's rules for M-mode. In every case, **check on the target** that
  touching the guard region really raises a fault.

### LoongArch

![LoongArch: like RISC-V, the stack grows down by software convention; process layout under Linux.](images/stack-heap-loongarch.svg)

- LoongArch, from Loongson, works much like RISC-V here: **no push or pop
  instructions**, the stack pointer is register `$r3` (`$sp`), and the standard
  calling convention (psABI) has the stack grow down with 16-byte alignment. The
  direction is a convention, not something the hardware enforces.
- It is mostly used in desktop, server and industrial systems, usually under Linux.
- **Not tested in the project's CI.** This section explains the memory layout only; it
  does not claim that the library has been verified on LoongArch.

## CA-04 Supply chain: the second way in

At run time the parser library has no dependencies, so the input is the only way in.
At build time there is a second one: the tools and data the library is built from.

| Entry point | Risk | Planned protection |
| --- | --- | --- |
| Unicode data used to generate `unicode_tables.hpp` | Tampered data changes how characters are classified | Pinned version and checksum; tables checked by unit tests |
| CI actions and runners | A compromised action changes what is built or released | Actions pinned to a full commit, least-privilege tokens (#131) |
| Compiler and toolchain | A compromised compiler adds hidden code ("Reflections on Trusting Trust") | A blocking second compiler (#141); see [tool confidence](../ISO26262-safety/tool-confidence.md#4-reflections-on-trusting-trust) |
| Release packages | A replaced archive reaches integrators | Checksums, an SBOM and signed releases (#131) |
| GoogleTest (tests only) | A compromised test framework hides failures | Pinned commit; never linked into the library; self-check (#142) |
