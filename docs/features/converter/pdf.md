# Local PDF tools

The typed PDF adapter uses the bundled qpdf 12.4.2 C API inside the isolated
converter worker. It accepts byte arrays only. The broker verifies every bundled
DLL against compiled hashes before loading the exact installed engine. PATH,
shell commands, network services, and developer-installed tools never enable it.

## Operations

| Operation | Options | Result |
| --- | --- | --- |
| Inspect | One source | Page count, normalized rotations, supported metadata |
| Split | One source | One validated PDF byte array per page |
| Merge | Multiple sources | Pages in source order; metadata must agree |
| Extract | Explicit one-based unique page indices | Selected pages in requested order |
| Reorder | Complete permutation of one-based page indices | All pages in requested order |
| Rotate | Absolute 0, 90, 180, or 270 degrees; optional page indices | Selected pages rotated; empty selection means all |
| Metadata | Title, Author, Subject, Keywords, Creator, Producer, CreationDate, ModDate, Trapped | Supplied fields replaced; other supported fields preserved |

The adapter supports static page trees. Catalog actions, forms, annotations,
outlines, page labels, XMP metadata, signatures, encryption, and other unsupported
document-level features fail closed. It never silently discards them. A signature
permission dictionary yields `pdf_signed`; unsupported form-based signature
containers yield `pdf_unsupported_catalog`. Supported metadata values must be
strings. Other representations, including name-valued metadata, are unavailable.

Inputs total at most 16 MiB, outputs total at most 64 MiB, source/page counts at
most 1,000, graph depth at most 64, and graph traversal at most 100,000 items per
page. Metadata values have a 4,096-byte bound. Worker memory, CPU, cancellation,
and zero-capability isolation are enforced by the broker process boundary.

Every output is reopened. Validation compares page count, normalized rotation,
metadata, and a structural representation of each page with its resources and
raw streams against the requested order. Indirect object numbers and page-parent
links do not participate. A mismatch discards all returned outputs and emits a
stable code such as `pdf_validation_page_order`. Parser diagnostics and document
content never become public error messages.

The adapter returns bytes, never writes a destination. The broker must validate
the installed package, serialize requests through its bounded worker protocol,
and publish through an exclusive atomic destination. Multi-output split requires
a broker-owned transaction or an explicitly selected archive output. Typed API
tests do not prove that the packaged UI or multi-output publishing is complete.

## Verification

`tests/local_converter/pdf_tests.cpp` exercises real synthetic PDF documents,
all seven operations, reopening, selection bounds, malformed input, unsupported
catalog actions, metadata conflicts, and engine availability. The test receives
the absolute staged qpdf DLL path; production loading must use the verified bundle.
