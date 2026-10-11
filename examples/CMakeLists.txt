# Data/documentation target only. The future CLI is tracked in issue #24.
add_custom_target(adaptive_json5_example_files SOURCES
    README.md manifest.json
    valid/strict.json
    valid/features.json5
    valid/unicode.json5
    valid/duplicates.json5
    valid/exact-numbers.json5
    valid/bom.json5
    valid/root-string.json5
    invalid/trailing-root.json5
    invalid/array-hole.json5
    invalid/token-suffix.json5
    invalid/unterminated-string.json5
    invalid/invalid-escape.json5
    invalid/leading-zero.json
    schema/device.schema.json
    schema/device-valid.json5
    schema/device-invalid.json5
)
