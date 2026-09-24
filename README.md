# RinLog

RinLog defines bounded logging records and interfaces for RinOS components.

## Public API contract

| Requirement | Contract |
| --- | --- |
| Purpose | RinLog defines bounded logging records and interfaces for RinOS components. |
| Supported API | The public C interface is `rinlog/log.h`, covering log records, bounded fields, and sink-facing data. |
| Unsupported API | RinLog does not guarantee persistence, transport, access control, automatic secret redaction, or a particular sink implementation. |
| ownership | Record data is bounded and supplied by the caller; consumers of records must copy any values they need beyond the documented call lifetime. |
| thread-safety | The record API is intended for independent calls. Sink synchronization and ordering are the responsibility of the sink implementation. |
| limits | Component, category, and event names are capped at 64 bytes; messages at 1024; records at 16 fields; keys at 64 bytes; values at 256; rendered output at 8192 bytes. |
| errors | Invalid or over-limit fields must be rejected or truncated as defined by the API. Callers should handle sink failure without relying on logging for control flow. |
| ABI stability | `rinlog/log.h` is the public C ABI. No compatibility promise across releases is published; rebuild consumers when updating the library. |
| security | Sensitive markers are metadata only. Sinks must honor them and apply their own redaction and access policy; logging must not be treated as a secret store. |
| build | Consume the public header from the RinOS build. No separate build/install command is documented. |
| test | No standalone test command is documented. Exercise the sink integration in the consuming target. |
