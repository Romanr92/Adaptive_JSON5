# Supported public C++ headers

`adaptive_json5/` contains consumer-facing C++17 declarations, organized by topic.
Consumers link `AdaptiveJSON5::parser`; the target supplies this include root.
No header here requires `src/includes/`. Template and compile-time definitions
remain in headers; ordinary implementations are compiled from `src/src/`.

The owned document/value/storage APIs are available. A public complete-document
parser entry remains P037 work; internal P033 engine declarations are not a
supported consumer parse API. See [source layout](../docs/adr/0002-public-private-source-layout.md).
