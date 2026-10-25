# Supported public C++ headers

`adaptive_json5/` contains consumer-facing C++17 declarations, organized by topic.
Consumers link `AdaptiveJSON5::parser`; the target supplies this include root.
No header here requires `core/includes/`. Template and compile-time definitions
remain in headers; ordinary implementations are compiled from `core/src/`.

The owned document/value/storage APIs and public
`parse_document()` complete-document entry are available. Internal P033 engine
declarations are not a supported consumer parse API. See [source layout](../core/docs/adr-0002-public-private-source-layout.md).
