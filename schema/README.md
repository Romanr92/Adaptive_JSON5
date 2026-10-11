# Optional schema component

Reserved for the independently buildable JSON Schema Draft-07 validator described
in the requirements. No validator is implemented yet. This directory is not a
dependency of the parser target, and no schema target is advertised until its
implementation and unit tests exist.

Keep schema errors separate from syntax errors and consumer policies. Runtime
schema resolution must remain explicitly controlled by the caller.
