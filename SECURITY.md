# Security policy

## Project status

Adaptive_JSON5 is still in development and is not yet recommended for production use.
Private vulnerability reporting will be switched on before the library is recommended
to others.

## Supported versions

Security fixes are released for these versions:

| Version | Security fixes |
| --- | --- |
| Latest release before 1.0.0 | Yes. Fixes go into the next release; older pre-1.0 releases are not patched. |
| Older releases before 1.0.0 | No. Update to the latest release. |

When 1.0.0 is released, this table will say which release lines receive security fixes
and for how long. The end of support for each line will be announced in advance.

## Reporting a security problem

**Please do not post the details of a vulnerability in a public issue.** Anyone,
including attackers, can read public issues before a fix exists.

Until private vulnerability reporting is switched on:

1. Open a short public issue that says only something like
   **"Possible security issue, please contact me"**. Include **no details**: no
   description of the problem, no triggering input, no affected function.
2. The maintainer then opens a draft security advisory in this repository and invites
   you to it. The details are discussed there, privately.
3. The fix is prepared in the private advisory and published together with the
   advisory when it is ready.

## What counts as a vulnerability

Report it privately as above if crafted input, or the use of the public API as
documented, can make the parser library:

- read or write memory outside its input or the memory it was given;
- crash, hang, or run past its configured limits;
- accept a document it should reject, or report a wrong value as success;
- leak memory contents.

Everything else, such as hardening ideas, documentation gaps or bugs with no security
impact, is welcome as a normal public issue.

## Scope

- In scope: the parser library `AdaptiveJSON5::parser` and its public headers.
- Out of scope: the hosted add-on (`AdaptiveJSON5::hosted`), examples, tests and tools,
  unless the problem also affects the parser library.

The library's threat analysis and the assumptions behind it are in
[`docs/ISO21434-cybersecurity/`](docs/ISO21434-cybersecurity/).
