# Tool confidence evaluation

| Item | Value |
| --- | --- |
| Work product | Software tool criteria evaluation (ISO 26262-8, clause 11) |
| Issue | [#138](https://github.com/Romanr92/Adaptive_JSON5/issues/138) |
| Status | Draft, not reviewed |
| Scope | Tools used to build, generate, check and package the parser library `AdaptiveJSON5::parser` |

## 1. Method

Each tool is rated with the two questions from ISO 26262-8, clause 11:

- **Tool impact (TI):** could a malfunction of the tool put a bug into the library,
  or fail to detect one? If not, TI1; if yes, TI2.
- **Tool error detection (TD):** if the tool did malfunction, how confident are we
  that it would be noticed or prevented? TD1 is high confidence, TD3 is low.

Together these give the **tool confidence level (TCL)**. A tool with TI1, or with
TI2 and TD1, gets **TCL1**, which needs no tool qualification. TCL2 and TCL3 would
need qualification methods from ISO 26262-8.

Note that a tool that only *checks* the code, such as a test framework, still counts
as TI2, because a malfunction could **fail to detect** a bug. Such tools reach TCL1
only through high error detection (TD1).

## 2. Evaluation

| Tool | Version | Used for | TI | Reason | TD | Reason | TCL |
| --- | --- | --- | --- | --- | --- | --- | --- |
| GCC (`g++-13` on Linux x86-64 and ARM64, MinGW-w64 GCC on Windows) | As installed by CI | Compiling the library and the tests | TI2 | A compiler fault can put a bug into the compiled code. | TD1 for the tested configurations | All unit, validation, sanitizer and fuzz runs execute the code exactly as this compiler built it, so a miscompilation on a tested path shows up as a test failure. The second compiler (V8) adds diversity. See section 3. | TCL1 |
| Clang (planned, V8) | As installed by CI | Second compiler; libFuzzer and sanitizers | TI2 | As for GCC. | TD1 | As for GCC, and the two compilers check each other. | TCL1 |
| Integrator's compiler and build options | Chosen by the integrator | Building the library for the target | — | — | — | **Not evaluated here.** The integrator evaluates their own compiler, options and target (see section 3). | Integrator |
| `tools/generate_unicode_tables.py` | Repository version | Generating the production header `core/includes/unicode/unicode_tables.hpp` from the pinned Unicode data | TI2 | It writes production code; a wrong table would misclassify characters. | TD1 | `core/tests/unicode/unicode_tables_tests.cpp` checks the generated tables against the Unicode data. #130 adds a byte-identical regeneration check in CI. | TCL1 |
| clang-format | 18.1.8 | Formatting production and test source | TI2 | It rewrites source files. | TD1 | It changes layout only. Every change is visible in the pull-request diff and is followed by a full build and test run. | TCL1 |
| CMake (and Ninja) | 3.25 or later | Configuring and building | TI2 | A wrong option or source list could build something different from what was reviewed. | TD1 | The tests run on what CMake built; `build_configuration_tests` checks the build settings. | TCL1 |
| `tools/package_release.py` | Repository version | Building the release archives | TI2 | A wrong file or setting could end up in a release. | TD1 | `tools/tests/test_package_release.py` checks the package contents; #130 runs it on pull requests. | TCL1 |
| GoogleTest | 1.17.0 (pinned commit) | Running the unit and validation tests | TI2 | It could report a pass for a failing test, so a bug would go undetected. | TD1, to be confirmed | Widely used; the same code is also exercised independently by sanitizers, fuzzing and coverage, so a wrong pass on its own is unlikely to hide a defect. Open item: a CI self-check that a deliberately failing test makes the job fail. | TCL1 |
| gcov and gcovr | gcov from the GCC used; gcovr 8.6 | Measuring coverage | TI2 | A wrong report could hide untested code. | TD1, to be confirmed | Coverage gaps are cross-checked against the test inventory in `docs/traceability.md`. | TCL1 |
| AddressSanitizer, UndefinedBehaviorSanitizer, libFuzzer (planned, V4 and V5) | From the compiler used | Detecting memory errors and undefined behaviour | TI2 | They could miss an error. | TD1 | They are extra detection on top of the unit tests, not the only one. | TCL1 |

**Result:** all tools are proposed at **TCL1**. The compiler used by the integrator is
the integrator's responsibility.

## 3. Compilers

A compiler can put a bug into code that is correct in the source. For this project,
the argument works like this:

- **For the configurations tested here**, every test runs on the code exactly as the
  compiler built it. A miscompilation on a tested path therefore shows up as a test
  failure. That is why the compilers get TD1, but only for the configurations,
  compiler versions and options that CI actually runs. Today all test jobs build
  `Debug` while release packages are `Release`; #130 closes that gap.
- **For the integrator's build**, the library is compiled again with the integrator's
  own compiler, options and target. The confidence evaluation for that compiler is the
  integrator's job. It is passed on as an obligation in the Safety Manual, and the
  shared-toolchain risk is recorded as DFA-03 in the
  [software safety analysis](software-safety-analysis.md).
- **Compiler diversity:** building and testing with a second compiler, Clang, on every
  pull request (V8 in the [verification strategy](verification-strategy.md)) makes it
  unlikely that a fault in a single compiler goes unnoticed.

## 4. "Reflections on Trusting Trust"

While evaluating the compiler, the owner raised the question of a hidden compiler
attack that goes back to Bell Labs. This section records that discussion and its
outcome.

### 4.1 The story

- **The idea is older than Thompson's lecture.** A U.S. Air Force security evaluation
  of Multics in the 1970s, by Paul Karger and Roger Schell, had already pointed out
  that a trap door could be put into a compiler and keep itself alive when the
  compiler is recompiled. Thompson credited this source.
- **Ken Thompson made it famous.** Thompson and Dennis Ritchie received the 1983 ACM
  Turing Award for Unix. Thompson's award lecture, *Reflections on Trusting Trust*,
  was published in *Communications of the ACM* in August 1984.
- **He described a compiler that cheats in two ways.** When it compiles the Unix
  `login` program, it secretly adds a back door that accepts a password known to the
  attacker. When it compiles a new version of the compiler itself, it secretly adds
  both tricks to the new compiler. After that, the malicious code can be removed from
  the compiler's source, and every future compiler built from clean source still
  carries it. Reading the source code, even of the compiler, shows nothing.
- **He actually built it.** It was not only a thought experiment. Thompson put it into
  the C compiler at Bell Labs. According to later accounts by Ritchie and Thompson,
  it was installed on a machine of another Bell Labs group as a test. The back door
  did appear in that machine's `login` program, and the group did not find it. It
  lasted several months, until someone replaced a compiler component with a copy from
  another system.
- **It was never distributed.** Thompson said the modified compiler was never
  distributed, and Ritchie stated that no such thing was ever included in any
  distributed version of Unix.

### 4.2 What this means for GCC

- **GCC does not come from Thompson's compiler.** GCC was written separately by
  Richard Stallman for the GNU project and first released in 1987. It has its own
  source lineage.
- **There is no known trusting-trust attack in GCC.** David A. Wheeler developed a
  countermeasure called **diverse double-compiling (DDC)**: compile the compiler's
  source with a second, independent compiler, use the result to compile the source
  again, and compare it bit for bit with the original compiler binary. He published
  it in 2005 (ACSAC) and, in his 2009 PhD dissertation at George Mason University,
  demonstrated it on GCC. The check confirmed that the GCC binary he tested
  corresponded to its source code.
- **The risk still exists in principle.** That check covers the version Wheeler
  tested, not every GCC binary anyone downloads today. The general lesson stands: a
  single toolchain should not be trusted blindly, and this applies to any tool or
  binary dependency, not only to compilers.

### 4.3 What the project does about it

- **A second compiler blocks every pull request.** The library is built and tested
  with Clang as well as GCC (V8), in #141. Clang and GCC
  have independent code bases, so a compromised or faulty compiler would have to
  produce a library that passes the same tests as the other compiler's build to go
  unnoticed. This is not a full DDC of the compiler itself, but it is cheap and
  directly protects the library.
- **The integrator's toolchain** remains the integrator's responsibility (section 3,
  DFA-03).
- **Cybersecurity:** a deliberately compromised toolchain is an attack, not only a
  random tool fault, so the topic is passed on to the ISO/SAE 21434 cybersecurity work
  in #139.

### 4.4 Sources

- Ken Thompson, "Reflections on Trusting Trust", *Communications of the ACM* 27(8),
  August 1984, pp. 761–763.
- David A. Wheeler, *Fully Countering Trusting Trust through Diverse
  Double-Compiling*, PhD dissertation, George Mason University, 2009:
  <https://dwheeler.com/trusting-trust/>, also arXiv:1004.5534.
- Dennis Ritchie's account in the Usenet group net.unix-wizards, November 1985, and
  the "back door" entry in the Jargon File, which records Thompson's confirmation that
  the modified compiler was never distributed.

## 5. Open items

- Confirm TD1 for GoogleTest and gcovr, for example with a CI self-check that a
  deliberately failing test fails the job.
- Add the Release test job (#130), so that the TD1 argument for the compilers also
  covers the shipped configuration.
