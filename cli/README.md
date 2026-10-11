# Optional command-line tools

Reserved for future host tools that use the parser and optional schema library.
No command-line executable is implemented yet. A future CLI must be independently
selectable and must not add host dependencies to the parser library.

The future `adaptive_json5_examples` host executable shall demonstrate the real
parser using [example files and their manifest](../examples/README.md), including
expected failures and requirement-specific observations. Issue #24 covers its
dispatch, diagnostics and unit-test seams alongside the validation CLI. It must
report unsupported/unimplemented demonstrations without claiming conformance.
